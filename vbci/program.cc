#include "program.h"

#include "thread.h"

#include <verona.h>

namespace vbci
{
  bool Program::load(std::filesystem::path& path)
  {
    file = path;
    std::ifstream f(file, std::ios::binary | std::ios::in | std::ios::ate);

    if (!f)
    {
      logging::Error() << file << ": could not open for reading" << std::endl;
      return {};
    }

    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);

    // Check the file header.
    constexpr auto header_size = 4 * sizeof(Code);
    constexpr auto header_words = header_size / sizeof(Code);

    if (size < header_size)
    {
      logging::Error() << file << ": too small" << std::endl;
      return false;
    }

    code.resize(header_words);
    f.read(reinterpret_cast<char*>(&code.at(0)), header_size);

    if constexpr (std::endian::native == std::endian::big)
    {
      for (size_t i = 0; i < header_words; i++)
        code[i] = std::byteswap(code[i]);
    }

    PC pc = 0;

    if (load_code(pc) != MagicNumber)
    {
      logging::Error() << file << ": does not start with the magic number"
                       << std::endl;
      return false;
    }

    if (load_code(pc) != CurrentVersion)
    {
      logging::Error() << file << ": has an unknown version number"
                       << std::endl;
      return false;
    }

    // Debug info.
    auto debug_offset = load_pc(pc);

    if (debug_offset > 0)
    {
      auto di_start = debug_offset * sizeof(Code);

      if (di_start > size)
      {
        logging::Error() << file << ": invalid debug offset" << std::endl;
        return false;
      }

      auto di_size = size - di_start;
      di.resize(di_size);
      f.seekg(di_start, std::ios::beg);
      f.read(reinterpret_cast<char*>(&di.at(0)), di_size);
      size = debug_offset * sizeof(Code);
    }

    if ((size % sizeof(Code)) != 0)
    {
      logging::Error() << file << ": invalid size" << std::endl;
      return false;
    }

    auto words = size / sizeof(Code);
    code.resize(words);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(&code.at(0)), size);

    if (!f)
    {
      logging::Error() << file << ": failed to read " << size << std::endl;
      return false;
    }

    if constexpr (std::endian::native == std::endian::big)
    {
      for (size_t i = 0; i < words; i++)
        code[i] = std::byteswap(code[i]);
    }

    return true;
  }

  bool Program::parse()
  {
    // Skip the file header.
    PC pc = 0;
    load_code(pc);
    load_code(pc);
    load_pc(pc);

    // Function headers.
    auto num_functions = load_u32(pc);
    functions.resize(num_functions);

    if (num_functions == 0)
    {
      logging::Error() << file << ": has no no functions" << std::endl;
      return false;
    }

    for (auto& f : functions)
    {
      if (!parse_function(f, pc))
        return false;
    }

    if (functions.at(MainFuncId).params.size() != 0)
    {
      logging::Error() << file << ": `main` must take zero parameters"
                       << std::endl;
      return false;
    }

    // Primitive classes.
    primitives.resize(NumPrimitiveClasses);

    for (auto& cls : primitives)
    {
      if (!parse_methods(cls, pc))
        return false;
    }

    // User-defined classes.
    auto num_classes = load_u32(pc);
    classes.resize(num_classes);
    Id class_id = 0;

    for (auto& cls : classes)
    {
      cls.class_id = class_id++;
      cls.debug_info = load_pc(pc);

      if (!parse_fields(cls, pc))
        return false;

      if (!parse_methods(cls, pc))
        return false;

      cls.calc_size();
    }

    return true;
  }

  bool Program::parse_function(Function& f, PC& pc)
  {
    // 8 bits for labels, 8 bits for parameters, 8 bits for registers.
    auto word = load_code(pc);
    auto labels = word & 0xFF;
    auto params = (word >> 8) & 0xFF;
    auto registers = (word >> 16) & 0xFF;

    if (labels == 0)
    {
      logging::Error() << file << ": function has no labels" << std::endl;
      return false;
    }

    f.labels.resize(labels);
    for (auto& label : f.labels)
      label = load_pc(pc);

    f.debug_info = load_pc(pc);
    f.params.resize(params);
    f.registers = registers;
    return true;
  }

  bool Program::parse_fields(Class& cls, PC& pc)
  {
    auto num_fields = load_u32(pc);
    cls.fields.reserve(num_fields);

    if (num_fields > MaxRegisters)
    {
      logging::Error() << file << ": too many fields in class" << std::endl;
      return false;
    }

    for (FieldIdx i = 0; i < num_fields; i++)
    {
      // This creates a mapping from a field name to an index into the object.
      Id name = load_u32(pc);
      cls.fields.emplace(name, i);
    }

    return true;
  }

  bool Program::parse_methods(Class& cls, PC& pc)
  {
    auto num_methods = load_u32(pc);
    cls.methods.reserve(num_methods);

    for (size_t i = 0; i < num_methods; i++)
    {
      // This creates a mapping from a method name to a function pointer.
      Id method_id = load_u32(pc);
      Id func_id = load_u32(pc);
      auto& func = functions.at(func_id);

      if (method_id == FinalMethodId)
      {
        if (func.params.size() != 1)
        {
          logging::Error() << file << ": finalizer must have one parameter"
                           << std::endl;
          return false;
        }
      }

      cls.methods.emplace(method_id, &func);
    }

    return true;
  }

  int Program::run()
  {
    using namespace verona::rt;
    auto& sched = Scheduler::get();
    (void)sched;

    return Thread::run(&functions.at(MainFuncId)).get_i32();
  }
}
