#include "program.h"

#include "object.h"

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

    auto size = f.tellg();
    f.seekg(0, std::ios::beg);

    if ((size % sizeof(Code)) != 0)
    {
      logging::Error() << file << ": not a multiple of " << sizeof(Code)
                       << " bytes" << std::endl;
      return false;
    }

    auto words = size / sizeof(Code);
    code.resize(words);
    f.read(reinterpret_cast<char*>(&code[0]), size);

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

    // Primitive classes.
    primitives.resize(NumPrimitiveClasses);

    for (auto& cls : primitives)
      parse_methods(cls, pc);

    // User-defined classes.
    auto num_classes = load_u32(pc);
    classes.resize(num_classes);

    for (auto& cls : classes)
    {
      parse_fields(cls, pc);
      parse_methods(cls, pc);
    }

    // Functions.
    auto num_functions = load_u32(pc);
    functions.resize(num_functions);

    for (auto& f : functions)
    {
      // 8 bits for labels, 8 bits for parameters.
      auto word = load_code(pc);
      auto labels = word & 0xFF;
      auto params = (word >> 8) & 0xFF;

      if (labels == 0)
      {
        logging::Error() << file << ": function has no labels" << std::endl;
        return false;
      }

      f.labels.resize(labels);
      for (auto& label : f.labels)
        label = load_pc(pc);

      // TODO: param types, return type
      f.params.resize(params);
    }

    return true;
  }

  void Program::parse_fields(Class& cls, PC& pc)
  {
    auto num_fields = load_u32(pc);
    cls.fields.reserve(num_fields);

    for (FieldIdx i = 0; i < num_fields; i++)
    {
      // This creates a mapping from a field name to an index into the object.
      // TODO: field types
      FieldId name = load_u32(pc);
      cls.fields.emplace(name, i);
    }
  }

  void Program::parse_methods(Class& cls, PC& pc)
  {
    auto num_methods = load_u32(pc);
    cls.methods.reserve(num_methods);

    for (size_t i = 0; i < num_methods; i++)
    {
      // This creates a mapping from a method name to a function pointer.
      MethodId method_id = load_u32(pc);
      FuncId func_id = load_u32(pc);
      cls.methods.emplace(method_id, &functions.at(func_id));
    }
  }
}
