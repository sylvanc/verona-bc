#include "program.h"

#include "array.h"
#include "ffi/ffi.h"
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

  Symbol& Program::symbol(size_t idx)
  {
    if (idx >= symbols.size())
      throw Value(Error::UnknownFFI);

    return symbols.at(idx);
  }

  Function* Program::function(size_t idx)
  {
    if (idx >= functions.size())
      return nullptr;

    return &functions.at(idx);
  }

  Class& Program::primitive(size_t idx)
  {
    // This will never be out of bounds.
    return primitives.at(idx);
  }

  Class& Program::cls(size_t idx)
  {
    if (idx >= classes.size())
      throw Value(Error::BadType);

    return classes.at(idx);
  }

  Value& Program::global(size_t idx)
  {
    if (idx >= globals.size())
      throw Value(Error::UnknownGlobal);

    return globals.at(idx);
  }

  int64_t Program::sleb(size_t& pc)
  {
    // This uses zigzag encoding.
    auto value = uleb(pc);
    return (value >> 1) ^ -(value & 1);
  }

  uint64_t Program::uleb(size_t& pc)
  {
    constexpr uint64_t max_shift = (sizeof(uint64_t) * 8) - 1;
    uint64_t value = 0;

    for (uint64_t shift = 0; shift <= max_shift; shift += 7)
    {
      value |= (uint64_t(content.at(pc)) & 0x7F) << shift;
      if ((content.at(pc++) & 0x80) == 0) [[likely]]
        break;
    }

    return value;
  }

  Array* Program::get_argv()
  {
    return argv;
  }

  Array* Program::get_string(size_t idx)
  {
    if (idx >= strings.size())
      throw Value(Error::UnknownGlobal);

    auto& str = strings.at(idx);
    auto str_size = str.size() + 1;
    auto arr = Array::create(
      new uint8_t[Array::size_of(str_size, ffi_type_uint8.size)],
      loc::Immutable,
      type::val(ValueType::U8),
      ValueType::U8,
      str_size,
      ffi_type_uint8.size);

    auto p = arr->get_pointer();
    std::memcpy(p, str.c_str(), str_size);
    arr->set_size(str_size - 1);
    return arr;
  }

  int Program::run(
    std::filesystem::path& path,
    size_t num_threads,
    std::vector<std::string> args)
  {
    file = path;

    if (!load())
      return -1;

    setup_argv(args);
    start_loop();
    auto& sched = verona::rt::Scheduler::get();
    sched.init(num_threads);
    auto ret = Thread::run_async(&functions.at(MainFuncId));
    sched.run();
    stop_loop();

    auto ret_val = ret.load();
    int exit_code;

    if (ret_val.is_error())
      exit_code = -1;
    else
      exit_code = ret_val.get_i32();

    ret.drop();
    return exit_code;
  }

  bool Program::typecheck(Id t1, Id t2)
  {
    // Checks if t1 <: t2.
    // If t2 is dynamic, anything is ok.
    if ((t1 == t2) || type::is_dyn(t2))
      return true;

    // Dyn is also used for invalid values and errors.
    if (type::is_dyn(t1))
      return false;

    // If t2 is an array, ref, or cown, t1 must be invariant.
    auto t2_mod = type::mod(t2);

    if (t2_mod)
    {
      if (type::mod(t1) != t2_mod)
        return false;

      t1 = type::no_mod(t1);
      t2 = type::no_mod(t2);
      return typecheck(t1, t2) && typecheck(t2, t1);
    }

    // Primitive types and classes have no subtypes.
    if (!type::is_def(classes.size(), t2))
      return false;

    // If t1 is a def, all of its types must be subtypes of t2.
    if (type::is_def(classes.size(), t1))
    {
      auto def = typedefs.at(type::def_idx(classes.size(), t1));
      for (auto t : def.type_ids)
      {
        if (!typecheck(t, t2))
          return false;
      }

      return true;
    }

    // If t1 is a subtype of any type in t2, that's sufficient.
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
    return {ValueType::Invalid, &ffi_type_pointer};
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

  std::string Program::di_class(Class& cls)
  {
    if (di == PC(-1))
      return std::format("class {}", cls.class_id);

    auto pc = di + cls.debug_info;
    return di_strings.at(uleb(pc));
  }

  std::string Program::di_field(Class& cls, size_t idx)
  {
    if (di == PC(-1))
      return std::to_string(idx);

    auto pc = di + cls.debug_info;
    uleb(pc);

    while (idx > 0)
      uleb(pc);

    return di_strings.at(uleb(pc));
  }

  void Program::setup_value_type()
  {
    ffi_type_value_elements.clear();
    ffi_type_value_elements.push_back(&ffi_type_uint64);
    ffi_type_value_elements.push_back(&ffi_type_uint64);
    ffi_type_value_elements.push_back(nullptr);

    ffi_type_value.size = sizeof(Value);
    ffi_type_value.alignment = alignof(Value);
    ffi_type_value.type = FFI_TYPE_STRUCT;
    ffi_type_value.elements = ffi_type_value_elements.data();
  }

  void Program::setup_argv(std::vector<std::string>& args)
  {
    auto type_u8 = type::val(ValueType::U8);
    auto arg_rep = layout_type_id(type_u8);

    auto type_arr_u8 = type::array(type_u8);
    auto argv_rep = layout_type_id(type_arr_u8);
    auto type_argv = type::def(classes.size(), typedefs.size());
    typedefs.resize(typedefs.size() + 1);
    typedefs.back().type_ids.push_back(type_arr_u8);

    argv = Array::create(
      new uint8_t[Array::size_of(args.size(), argv_rep.second->size)],
      loc::Stack,
      type_argv,
      argv_rep.first,
      args.size(),
      argv_rep.second->size);

    for (size_t i = 0; i < args.size(); i++)
    {
      auto str = args.at(i);
      auto str_size = str.size() + 1;
      auto arg = Array::create(
        new uint8_t[Array::size_of(str_size, arg_rep.second->size)],
        loc::Stack,
        type_u8,
        arg_rep.first,
        str_size,
        arg_rep.second->size);

      auto p = arg->get_pointer();
      std::memcpy(p, str.c_str(), str_size);
      arg->set_size(str_size - 1);

      auto arg_value = Value(arg);
      argv->store(true, i, arg_value);
    }

    argv->immortalize();
  }

  bool Program::load()
  {
    content.clear();
    typedefs.clear();
    functions.clear();
    primitives.clear();
    classes.clear();
    globals.clear();

    libs.clear();
    symbols.clear();
    setup_value_type();

    argv = nullptr;

    di = PC(-1);
    di_compilation_path = 0;
    di_strings.clear();
    source_files.clear();

    std::ifstream f(file, std::ios::binary | std::ios::in | std::ios::ate);

    if (!f)
    {
      logging::Error() << file << ": couldn't load" << std::endl;
      return false;
    }

    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    content.resize(size);
    f.read(reinterpret_cast<char*>(&content.at(0)), size);

    if (!f)
    {
      logging::Error() << file << ": couldn't read" << std::endl;
      return false;
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

    // String table.
    string_table(pc, strings);

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

    // FFI information.
    auto num_libs = uleb(pc);
    for (size_t i = 0; i < num_libs; i++)
      libs.push_back(strings.at(uleb(pc)));

    auto num_symbols = uleb(pc);
    for (size_t i = 0; i < num_symbols; i++)
    {
      auto& lib = libs.at(uleb(pc));
      auto& name = strings.at(uleb(pc));
      auto& version = strings.at(uleb(pc));
      bool vararg = uleb(pc);
      auto func = lib.symbol(name, version);

      if (!func)
      {
        logging::Error() << file << ": couldn't load symbol " << name << "@"
                         << version << std::endl;
        return false;
      }

      symbols.push_back(Symbol(func, vararg));
      auto& symbol = symbols.back();

      auto num_params = uleb(pc);
      for (size_t j = 0; j < num_params; j++)
      {
        auto type_id = uleb(pc);
        auto rep = layout_type_id(type_id);
        symbol.param(type_id, rep.first, rep.second);
      }

      auto type_id = uleb(pc);
      auto rep = layout_type_id(type_id);

      if (rep.first == ValueType::Invalid)
        rep.second = &ffi_type_value;

      symbol.ret(type_id, rep.first, rep.second);

      if (!symbol.prepare())
      {
        logging::Error() << file << ": couldn't prepare symbol " << name
                         << std::endl;
        return false;
      }
    }

    // Functions.
    auto num_functions = uleb(pc);
    functions.resize(num_functions);

    if (num_functions == 0)
    {
      logging::Error() << file << ": has no functions" << std::endl;
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

    for (auto& cls : primitives)
    {
      if (!fixup_methods(cls))
        return false;
    }

    for (auto& cls : classes)
    {
      if (!fixup_methods(cls))
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

    // Function label locations are relative to the code section. Make them
    // absolute.
    for (auto& func : functions)
    {
      for (auto& label : func.labels)
        label += pc;
    }

    return true;
  }

  bool Program::parse_function(Function& f, PC& pc)
  {
    f.registers = uleb(pc);
    f.debug_info = uleb(pc);

    // Function signature.
    auto params = uleb(pc);
    f.param_types.resize(params);

    for (size_t i = 0; i < params; i++)
      f.param_types.at(i) = uleb(pc);

    f.return_type = uleb(pc);

    // Labels.
    f.labels.resize(uleb(pc));

    if (f.labels.empty())
    {
      logging::Error() << file << ": function has no labels" << std::endl;
      return false;
    }

    for (auto& label : f.labels)
      label = uleb(pc);

    return true;
  }

  bool Program::parse_fields(Class& cls, PC& pc)
  {
    std::vector<ffi_type*> ffi_types;
    auto num_fields = uleb(pc);
    cls.field_map.reserve(num_fields);
    cls.fields.resize(num_fields);
    ffi_types.resize(num_fields);

    for (size_t i = 0; i < num_fields; i++)
    {
      auto& f = cls.fields.at(i);
      cls.field_map.emplace(uleb(pc), i);
      f.type_id = uleb(pc);

      auto rep = layout_type_id(f.type_id);
      f.value_type = rep.first;
      ffi_types.at(i) = rep.second;
    }

    ffi_types.push_back(nullptr);
    cls.calc_size(ffi_types);
    return true;
  }

  bool Program::parse_methods(Class& cls, PC& pc)
  {
    auto num_methods = uleb(pc);
    cls.methods.reserve(num_methods);

    for (size_t i = 0; i < num_methods; i++)
    {
      auto method_id = uleb(pc);
      auto func_id = uleb(pc);
      cls.methods.emplace(method_id, reinterpret_cast<Function*>(func_id));
    }

    return true;
  }

  bool Program::fixup_methods(Class& cls)
  {
    for (auto& method : cls.methods)
    {
      auto idx = reinterpret_cast<size_t>(method.second);
      auto& func = functions.at(idx);
      method.second = &func;

      if (method.first == FinalMethodId)
      {
        if (func.param_types.size() != 1)
        {
          logging::Error() << file << ": finalizer must have one parameter"
                           << std::endl;
          return false;
        }
      }
    }

    return true;
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
    table.clear();
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
