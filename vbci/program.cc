#include "program.h"

#include "thread.h"

#include <dlfcn.h>
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

  Program& Program::get()
  {
    static Program program;
    return program;
  }

  Function* Program::function(Id id)
  {
    if (id >= functions.size())
      throw Value(Error::UnknownFunction);

    return &functions.at(id);
  }

  Class& Program::primitive(Id id)
  {
    return primitives.at(id);
  }

  Class& Program::cls(Id id)
  {
    return classes.at(id);
  }

  Value& Program::global(Id id)
  {
    if (id >= globals.size())
      throw Value(Error::UnknownGlobal);

    return globals.at(id);
  }

  Code Program::load_code(PC& pc)
  {
    return code[pc++];
  }

  PC Program::load_pc(PC& pc)
  {
    return load_u64(pc);
  }

  int16_t Program::load_i16(PC& pc)
  {
    return code[pc++];
  }

  int32_t Program::load_i32(PC& pc)
  {
    return code[pc++];
  }

  int64_t Program::load_i64(PC& pc)
  {
    auto lo = code[pc++];
    auto hi = code[pc++];
    return (int64_t(hi) << 32) | int64_t(lo);
  }

  uint16_t Program::load_u16(PC& pc)
  {
    return code[pc++];
  }

  uint32_t Program::load_u32(PC& pc)
  {
    return code[pc++];
  }

  uint64_t Program::load_u64(PC& pc)
  {
    auto lo = code[pc++];
    auto hi = code[pc++];
    return (uint64_t(hi) << 32) | uint64_t(lo);
  }

  float Program::load_f32(PC& pc)
  {
    return std::bit_cast<float>(code[pc++]);
  }

  double Program::load_f64(PC& pc)
  {
    auto lo = code[pc++];
    auto hi = code[pc++];
    return std::bit_cast<double>((static_cast<uint64_t>(hi) << 32) | lo);
  }

  int Program::run(std::filesystem::path& path)
  {
    file = path;

    if (!load())
      return -1;

    using namespace verona::rt;
    auto& sched = Scheduler::get();
    (void)sched;

    auto ret = Thread::run(&functions.at(MainFuncId));
    std::cout << ret.to_string() << std::endl;
    return 0;
  }

  std::string Program::debug_info(Function* func, PC pc)
  {
    if (di == PC(-1))
      return std::format(" --> function {}:{}", static_cast<void*>(func), pc);

    constexpr auto no_value = size_t(-1);
    auto di_file = no_value;
    auto di_offset = 0;
    auto cur_pc = func->labels.at(0);
    auto di_pc = di + func->debug_info;

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
        return std::format(" --> function {}:{}", static_cast<void*>(func), pc);
      }
    }

    auto filename = di_strings.at(di_file);
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
    if (di == PC(-1))
      return std::format("function {}", static_cast<void*>(func));

    auto pc = di + func->debug_info;
    return di_strings.at(uleb(pc));
  }

  std::string Program::di_class(Class* cls)
  {
    if (di == PC(-1))
      return std::format("class {}", cls->class_id);

    auto pc = di + cls->debug_info;
    return di_strings.at(uleb(pc));
  }

  std::string Program::di_field(Class* cls, FieldIdx idx)
  {
    if (di == PC(-1))
      return std::to_string(idx);

    auto pc = di + cls->debug_info;
    uleb(pc);

    while (idx > 0)
      uleb(pc);

    return di_strings.at(uleb(pc));
  }

  bool Program::load()
  {
    content.clear();
    code = nullptr;
    di = PC(-1);

    functions.clear();
    primitives.clear();
    classes.clear();
    globals.clear();
    libs.clear();
    di_compilation_path = 0;
    di_strings.clear();
    source_files.clear();

    std::ifstream f(file, std::ios::binary | std::ios::in | std::ios::ate);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    content.resize(size);
    f.read(reinterpret_cast<char*>(&content.at(0)), size);

    if (!f)
    {
      logging::Error() << file << ": couldn't load" << std::endl;
      return {};
    }

    PC pc = 0;

    if (uleb(pc) != MagicNumber)
    {
      logging::Error() << file << ": does not start with the magic number"
                       << std::endl;
      return false;
    }

    if (uleb(pc) != CurrentVersion)
    {
      logging::Error() << file << ": has an unknown version number"
                       << std::endl;
      return false;
    }

    // FFI information.
    string_table(pc, ffi_strings);

    auto num_libs = uleb(pc);
    for (size_t i = 0; i < num_libs; i++)
      libs.push_back(ffi_strings.at(uleb(pc)));

    auto num_symbols = uleb(pc);
    for (size_t i = 0; i < num_symbols; i++)
    {
      auto& lib = libs.at(uleb(pc));
      auto& name = ffi_strings.at(uleb(pc));
      auto symbol = lib.symbol(name);

      // TODO: parameter types.
      auto num_params = uleb(pc);
      for (size_t j = 0; j < num_params; j++)
        uleb(pc);

      // TODO: return type.
      uleb(pc);

      // TODO: prep the symbol.
      (void)symbol;
    }

    // Function headers.
    auto num_functions = uleb(pc);
    functions.resize(num_functions);

    if (num_functions == 0)
    {
      logging::Error() << file << ": has no no functions" << std::endl;
      return false;
    }

    for (auto& func : functions)
    {
      if (!parse_function(func, pc))
        return false;
    }

    if (functions.at(MainFuncId).param_types.size() != 0)
    {
      logging::Error() << file << ": `main` must take zero parameters"
                       << std::endl;
      return false;
    }

    // Typedefs.
    auto num_typedefs = uleb(pc);
    typedefs.resize(num_typedefs);

    for (size_t i = 0; i < num_typedefs; i++)
    {
      auto& def = typedefs.at(i);
      auto num_types = uleb(pc);
      def.type_ids.resize(num_types);

      for (size_t j = 0; j < num_types; j++)
        def.type_ids.at(j) = uleb(pc);
    }

    // Primitive classes.
    primitives.resize(NumPrimitiveClasses);

    for (auto& cls : primitives)
    {
      cls.class_id = Id(-1);
      cls.debug_info = size_t(-1);

      if (!parse_methods(cls, pc))
        return false;
    }

    // User-defined classes.
    auto num_classes = uleb(pc);
    classes.resize(num_classes);
    Id class_id = 0;

    for (auto& cls : classes)
    {
      cls.class_id = class_id++;
      cls.debug_info = uleb(pc);

      if (!parse_fields(cls, pc))
        return false;

      if (!parse_methods(cls, pc))
        return false;

      cls.calc_size();
    }

    // Debug info.
    auto debug_info_size = uleb(pc);

    if (debug_info_size > 0)
    {
      di = pc;
      string_table(pc, di_strings);
      di_compilation_path = uleb(pc);
      pc = di + debug_info_size;
    }

    // Skip padding.
    pc += (sizeof(Code) - (pc % sizeof(Code))) % sizeof(Code);

    if (((size - pc) % sizeof(Code)) != 0)
    {
      logging::Error() << file << ": invalid file size" << std::endl;
      return false;
    }

    code = reinterpret_cast<Code*>(content.data() + pc);
    auto words = (size - pc) / sizeof(Code);

    if constexpr (std::endian::native == std::endian::big)
    {
      for (size_t i = 0; i < words; i++)
        code[i] = std::byteswap(code[i]);
    }

    // Patch the function headers with offsets.
    pc = 0;

    for (auto& func : functions)
    {
      for (auto& label : func.labels)
        label = load_pc(pc);

      func.debug_info = load_pc(pc);
    }

    return true;
  }

  bool Program::parse_function(Function& f, PC& pc)
  {
    auto params = uleb(pc);
    auto registers = uleb(pc);
    auto labels = uleb(pc);

    if (labels == 0)
    {
      logging::Error() << file << ": function has no labels" << std::endl;
      return false;
    }

    // Function signature.
    f.param_types.resize(params);
    for (size_t i = 0; i < params; i++)
      f.param_types.at(i) = uleb(pc);

    f.return_type = uleb(pc);
    f.registers = registers;
    f.labels.resize(labels);
    return true;
  }

  bool Program::parse_fields(Class& cls, PC& pc)
  {
    auto num_fields = uleb(pc);
    cls.fields.reserve(num_fields);

    if (num_fields > MaxRegisters)
    {
      logging::Error() << file << ": too many fields in class" << std::endl;
      return false;
    }

    for (FieldIdx i = 0; i < num_fields; i++)
    {
      cls.fields.emplace(uleb(pc), i);
      cls.field_types.push_back(uleb(pc));
    }

    return true;
  }

  bool Program::parse_methods(Class& cls, PC& pc)
  {
    auto num_methods = uleb(pc);
    cls.methods.reserve(num_methods);

    for (size_t i = 0; i < num_methods; i++)
    {
      // This creates a mapping from a method name to a function pointer.
      Id method_id = uleb(pc);
      Id func_id = uleb(pc);
      auto& func = functions.at(func_id);

      if (method_id == FinalMethodId)
      {
        if (func.param_types.size() != 1)
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

  size_t Program::uleb(size_t& pc)
  {
    size_t value = 0;
    size_t shift = 0;

    while (true)
    {
      auto byte = content.at(pc++);
      value |= (size_t(byte) & 0x7F) << shift;

      if ((byte & 0x80) == 0)
        break;

      shift += 7;
    }

    return value;
  }

  std::string Program::str(size_t& pc)
  {
    auto size = uleb(pc);
    auto str = std::string(content.begin() + pc, content.begin() + pc + size);
    pc += size;
    return str;
  }

  void Program::string_table(size_t& pc, std::vector<std::string>& table)
  {
    auto count = uleb(pc);
    table.reserve(count);

    for (size_t i = 0; i < count; i++)
      table.push_back(str(pc));
  }

  SourceFile* Program::get_source_file(const std::string& path)
  {
    auto find = source_files.find(path);
    if (find != source_files.end())
      return &find->second;

    auto filename =
      std::filesystem::path(di_strings.at(di_compilation_path)) / path;
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
