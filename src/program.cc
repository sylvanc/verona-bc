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
      // 8 bits for params.
      auto word = load_u32(pc);

      if (word > registers)
      {
        logging::Error() << file << ": too many parameters" << std::endl;
        return false;
      }

      // TODO: param types, return type
      f.params.resize(word);
      f.pc = load_u64(pc);
    }

    // Build primitive type descriptors.
    auto num_primitives = static_cast<size_t>(ValueType::F64) + 1;
    primitives.resize(num_primitives);

    for (auto& p : primitives)
    {
      if (!parse_type(p, pc))
      {
        logging::Error() << file << ": failed to parse primitive type"
                         << std::endl;
        return false;
      }
    }

    // Build user-defined type descriptors.
    auto num_types = load_u32(pc);
    types.resize(num_types);

    for (auto& t : types)
    {
      if (!parse_type(t, pc))
      {
        logging::Error() << file << ": failed to parse type" << std::endl;
        return false;
      }
    }

    return true;
  }

  bool Program::parse_type(TypeDesc& t, PC& pc)
  {
    auto num_fields = load_u16(pc);
    t.fields.reserve(num_fields);

    for (FieldIdx i = 0; i < num_fields; i++)
    {
      // This creates a mapping from a field name to an index into the object.
      FieldId name = load_u32(pc);
      t.fields.emplace(name, i);
    }

    auto num_methods = load_u32(pc);
    t.methods.reserve(num_methods);

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

      t.methods.emplace(name, &functions.at(idx));
    }

    return true;
  }
}
