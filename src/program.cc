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

    if (load_u32(pc) != 0xDEADBEEF)
    {
      logging::Error() << file << ": does not start with the magic number"
                       << std::endl;
      return false;
    }

    if (load_u32(pc) != 0)
    {
      logging::Error() << file << ": has an unknown version number"
                       << std::endl;
      return false;
    }

    // Start with functions.
    auto num_functions = load_u32(pc);
    functions.resize(num_functions);

    for (auto& f : functions)
    {
      // 8 bits for labels, 8 bits for parameters.
      auto word = load_u32(pc);
      auto labels = static_cast<size_t>(word & 0xFF);

      if (!labels)
      {
        logging::Error() << file << ": function has no labels" << std::endl;
        return false;
      }

      f.labels.resize(labels);
      for (auto& label : f.labels)
        label = static_cast<PC>(load_u64(pc));

      // TODO: param types, return type
      auto params = static_cast<size_t>((word >> 8) & 0xFF);
      f.params.resize(params);
    }

    // Build primitive type descriptors.
    auto num_primitives = static_cast<size_t>(ValueType::F64) + 1;
    primitives.resize(num_primitives);

    for (auto& cls : primitives)
    {
      if (!parse_class(cls, pc))
      {
        logging::Error() << file << ": failed to parse primitive class"
                         << std::endl;
        return false;
      }
    }

    // Build user-defined classes.
    auto num_classes = load_u32(pc);
    classes.resize(num_classes);

    for (auto& cls : classes)
    {
      if (!parse_class(cls, pc))
      {
        logging::Error() << file << ": failed to parse class" << std::endl;
        return false;
      }
    }

    return true;
  }

  bool Program::parse_class(Class& cls, PC& pc)
  {
    auto num_fields = load_u16(pc);
    cls.fields.reserve(num_fields);

    for (FieldIdx i = 0; i < num_fields; i++)
    {
      // This creates a mapping from a field name to an index into the object.
      FieldId name = load_u32(pc);
      cls.fields.emplace(name, i);
    }

    auto num_methods = load_u32(pc);
    cls.methods.reserve(num_methods);

    for (FuncId i = 0; i < num_methods; i++)
    {
      // This creates a mapping from a method name to a function pointer.
      FuncId name = load_u32(pc);
      size_t idx = load_u32(pc);

      if (idx >= functions.size())
      {
        logging::Error() << file << ": function ID out of bounds" << std::endl;
        return false;
      }

      cls.methods.emplace(name, &functions.at(idx));
    }

    return true;
  }
}
