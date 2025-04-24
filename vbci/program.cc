#include "program.h"

#include "thread.h"

#include <format>
#include <verona.h>

namespace vbci
{
  std::pair<size_t, size_t> SourceFile::linecol(size_t pos)
  {
    // Lines and columns are 0-indexed.
    auto it = std::lower_bound(lines.begin(), lines.end(), pos);

    auto line = it - lines.begin();
    auto col = pos;

    if (it != lines.begin())
      col -= *(it - 1) + 1;

    return {line, col};
  }

  std::string SourceFile::line(size_t line)
  {
    // Lines are 0-indexed.
    if (line > lines.size())
      return {};

    size_t start = 0;
    auto end = contents.size();

    if (line > 0)
      start = lines.at(line - 1) + 1;

    if (line < lines.size())
      end = lines.at(line);

    auto ret = contents.substr(start, end - start);
    std::erase(ret, '\r');
    return ret;
  }

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

    auto ret = Thread::run(&functions.at(MainFuncId));
    std::cout << ret.to_string() << std::endl;
    return 0;
  }

  std::string Program::debug_info(Function* func, PC pc)
  {
    if (di.size() == 0)
      return "no debug info";

    constexpr auto no_value = size_t(-1);
    auto di_file = no_value;
    auto di_offset = 0;
    auto cur_pc = func->labels.at(0);
    auto di_pc = func->debug_info;

    // Read past the function name and the register names.
    uleb(di_pc);

    for (size_t i = 0; i < func->registers; i++)
      uleb(di_pc);

    while (cur_pc < pc)
    {
      auto u = uleb(di_pc);
      auto op = u & 0x03;
      u >>= 2;

      if (op == +DIOp::File)
      {
        di_file = u;
        di_offset = 0;
      }
      else if (op == +DIOp::Offset)
      {
        di_offset += u;
        cur_pc++;
      }
      else if (op == +DIOp::Skip)
      {
        cur_pc += u;
      }
      else
      {
        logging::Error() << file << ": unknown debug info op" << std::endl;
        return "<unknown>";
      }
    }

    auto filename = di_string(di_file);
    auto source = get_source_file(filename);

    if (!source)
      return std::format(" --> {}:{}", filename, std::to_string(di_offset));

    auto [line, col] = source->linecol(di_offset);
    auto src_line = source->line(line);
    auto caret = src_line.substr(0, col);

    std::replace_if(
      caret.begin(),
      caret.end(),
      [](unsigned char ch) { return ch != '\t'; },
      ' ');

    return std::format(
      " --> {}:{}:{}\n  | {}\n  | {}^",
      filename,
      line + 1,
      col + 1,
      src_line,
      caret);
  }

  std::string Program::di_function(Function* func)
  {
    if (di.size() == 0)
      return "?";

    auto pc = func->debug_info;
    return di_string(uleb(pc));
  }

  std::string Program::di_class(Class* cls)
  {
    if (di.size() == 0)
      return std::to_string(cls->class_id);

    auto pc = cls->debug_info;
    return di_string(uleb(pc));
  }

  std::string Program::di_field(Class* cls, FieldIdx idx)
  {
    if (di.size() == 0)
      return std::to_string(idx);

    auto pc = cls->debug_info;
    uleb(pc);

    while (idx > 0)
      uleb(pc);

    return di_string(uleb(pc));
  }

  std::string Program::di_string(size_t idx)
  {
    if (di_strings.size() == 0)
    {
      size_t di_pc = 0;
      auto count = uleb(di_pc);
      di_strings.reserve(count);

      for (size_t i = 0; i < count; i++)
      {
        auto size = uleb(di_pc);
        di_strings.push_back(
          std::string(di.begin() + di_pc, di.begin() + di_pc + size));
        di_pc += size;
      }

      di_compilation_path = uleb(di_pc);
    }

    if (idx >= di_strings.size())
      return "<unknown>";

    return di_strings.at(idx);
  }

  size_t Program::uleb(size_t& pc)
  {
    size_t value = 0;
    size_t shift = 0;

    while (true)
    {
      auto byte = di.at(pc++);
      value |= (byte & 0x7F) << shift;

      if ((byte & 0x80) == 0)
        break;

      shift += 7;
    }

    return value;
  }

  SourceFile* Program::get_source_file(const std::string& path)
  {
    auto find = source_files.find(path);
    if (find != source_files.end())
      return &find->second;

    auto filename =
      std::filesystem::path(di_string(di_compilation_path)) / path;
    std::ifstream f(filename, std::ios::binary | std::ios::in | std::ios::ate);

    if (!f)
      return nullptr;

    auto size = f.tellg();
    f.seekg(0, std::ios::beg);

    auto& source = source_files[path];
    source.contents.resize(static_cast<std::size_t>(size));
    f.read(&source.contents.at(0), size);

    auto pos = source.contents.find('\n');

    while (pos != std::string::npos)
    {
      source.lines.push_back(pos);
      pos = source.contents.find('\n', pos + 1);
    }

    return &source;
  }
}
