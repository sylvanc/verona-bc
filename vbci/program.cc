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

  Symbol& Program::symbol(Id id)
  {
    return symbols.at(id);
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

  bool Program::typecheck(Id t1, Id t2)
  {
    // Checks if t1 <: t2. t1 is a concrete type, so won't be dyn or a typedef.
    // However, t1 can be an array, ref, or cown of a typedef. Dyn is used for
    // invalid values and errors.
    assert(!type::is_def(classes.size(), t1));
    if (type::is_dyn(t1))
      return false;

    // If t2 is dynamic, anything is ok.
    if ((t1 == t2) || type::is_dyn(t2))
      return true;

    // If t2 is an array, ref, or cown, it doesn't matter what type it's
    // modifying. If t1 isn't identical, it's not a subtype. Primitive types and
    // classes have no subtypes. So t2 must be an unmodified typedef.
    if (!type::is_def(classes.size(), t2))
      return false;

    // If t2 is a subtype of any type in the typedef, that's sufficient.
    auto def = typedefs.at(type::def_idx(classes.size(), t2));
    for (auto t : def.type_ids)
    {
      if (typecheck(t1, t))
        return true;
    }

    return false;
  }

  std::pair<ValueType, ffi_type*> Program::layout_type_id(Id type_id)
  {
    if (type::is_val(type_id))
    {
      auto t = type::val(type_id);

      switch (t)
      {
        case ValueType::None:
          return {ValueType::None, &ffi_type_void};
        case ValueType::Bool:
          return {ValueType::Bool, &ffi_type_uint8};
        case ValueType::I8:
          return {ValueType::I8, &ffi_type_sint8};
        case ValueType::I16:
          return {ValueType::I16, &ffi_type_sint16};
        case ValueType::I32:
          return {ValueType::I32, &ffi_type_sint32};
        case ValueType::I64:
          return {ValueType::I64, &ffi_type_sint64};
        case ValueType::U8:
          return {ValueType::U8, &ffi_type_uint8};
        case ValueType::U16:
          return {ValueType::U16, &ffi_type_uint16};
        case ValueType::U32:
          return {ValueType::U32, &ffi_type_uint32};
        case ValueType::U64:
          return {ValueType::U64, &ffi_type_uint64};
        case ValueType::F32:
          return {ValueType::F32, &ffi_type_float};
        case ValueType::F64:
          return {ValueType::F64, &ffi_type_double};
        case ValueType::ILong:
          return {ValueType::ILong, &ffi_type_slong};
        case ValueType::ULong:
          return {ValueType::ULong, &ffi_type_ulong};
        case ValueType::ISize:
          return {
            ValueType::ISize,
            sizeof(ssize_t) == 4 ? &ffi_type_sint32 : &ffi_type_sint64};
        case ValueType::USize:
          return {
            ValueType::USize,
            sizeof(size_t) == 4 ? &ffi_type_uint32 : &ffi_type_uint64};
        case ValueType::Ptr:
          return {ValueType::Ptr, &ffi_type_pointer};
        default:
          assert(false);
          return {ValueType::Invalid, &ffi_type_void};
      }
    }
    else if (type::is_cls(classes.size(), type_id))
    {
      return {ValueType::Object, &ffi_type_pointer};
    }
    else if (type::is_def(classes.size(), type_id))
    {
      // If all elements of the union have the same representation, use it.
      auto& def = typedefs.at(type::def_idx(classes.size(), type_id));
      auto rep = layout_type_id(def.type_ids.at(0));
      bool ok = true;

      for (size_t i = 0; i < def.type_ids.size(); i++)
      {
        if (rep != layout_type_id(def.type_ids.at(i)))
        {
          ok = false;
          break;
        }
      }

      if (ok)
        return rep;
    }
    else if (type::is_array(type_id))
    {
      return {ValueType::Array, &ffi_type_pointer};
    }

    // Dyn, cown, ref, array ref, cown ref.
    return {ValueType::Invalid, &value_type};
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

  bool Program::setup_value_type()
  {
    std::vector<ffi_type*> field_types;
    field_types.push_back(&ffi_type_uint64);
    field_types.push_back(&ffi_type_uint64);

    std::vector<size_t> field_offsets;
    field_offsets.resize(field_types.size());

    value_type.size = 0;
    value_type.alignment = 0;
    value_type.type = FFI_TYPE_STRUCT;
    value_type.elements = field_types.data();

    return ffi_get_struct_offsets(FFI_DEFAULT_ABI, &value_type, nullptr) ==
      FFI_OK;
  }

  bool Program::load()
  {
    if (!setup_value_type())
      return false;

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
    std::vector<std::string> ffi_strings;
    string_table(pc, ffi_strings);

    auto num_libs = uleb(pc);
    for (size_t i = 0; i < num_libs; i++)
      libs.push_back(ffi_strings.at(uleb(pc)));

    auto num_symbols = uleb(pc);
    for (size_t i = 0; i < num_symbols; i++)
    {
      auto& lib = libs.at(uleb(pc));
      auto& name = ffi_strings.at(uleb(pc));
      auto func = lib.symbol(name);

      if (!func)
      {
        logging::Error() << file << ": couldn't load symbol " << name
                         << std::endl;
        return false;
      }

      symbols.push_back(Symbol(func));
      auto& symbol = symbols.back();

      auto num_params = uleb(pc);
      for (size_t j = 0; j < num_params; j++)
      {
        auto type_id = uleb(pc);
        auto rep = layout_type_id(type_id);
        symbol.param(type_id, rep.second);
      }

      auto type_id = uleb(pc);
      auto rep = layout_type_id(type_id);
      symbol.ret(rep.first, rep.second);

      if (!symbol.prepare())
      {
        logging::Error() << file << ": couldn't prepare symbol " << name
                         << std::endl;
        return false;
      }
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
    std::vector<ffi_type*> ffi_types;
    auto num_fields = uleb(pc);
    cls.field_map.reserve(num_fields);
    cls.fields.resize(num_fields);
    ffi_types.resize(num_fields);

    if (num_fields > MaxRegisters)
    {
      logging::Error() << file << ": too many fields in class" << std::endl;
      return false;
    }

    for (FieldIdx i = 0; i < num_fields; i++)
    {
      auto& f = cls.fields.at(i);
      cls.field_map.emplace(uleb(pc), i);
      f.type_id = uleb(pc);

      auto rep = layout_type_id(f.type_id);
      f.value_type = rep.first;
      ffi_types.at(i) = rep.second;
    }

    cls.calc_size(ffi_types);
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
