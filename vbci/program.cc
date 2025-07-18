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

  Class& Program::cls(TypeId type_id)
  {
    if (!type_id.is_class())
      throw Value(Error::BadType);

    return classes.at(type_id.cls());
  }

  Value& Program::global(size_t idx)
  {
    if (idx >= globals.size())
      throw Value(Error::UnknownGlobal);

    return globals.at(idx);
  }

  ComplexType& Program::complex_type(size_t idx)
  {
    if (idx >= complex_types.size())
      throw Value(Error::BadType);

    return complex_types.at(idx);
  }

  ffi_type* Program::value_type()
  {
    return &ffi_type_value;
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
      TypeId::val(ValueType::U8),
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
    {
      LOG(Debug) << ret.to_string();
      exit_code = -1;
    }
    else
    {
      exit_code = ret_val.get_i32();
    }

    ret.drop();
    return exit_code;
  }

  std::pair<ValueType, ffi_type*> Program::layout_type_id(TypeId type_id)
  {
    if (type_id.is_val())
    {
      auto t = type_id.val();

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
    else if (type_id.is_class())
    {
      return {ValueType::Object, &ffi_type_pointer};
    }
    else if (type_id.is_array())
    {
      return {ValueType::Array, &ffi_type_pointer};
    }
    else if (type_id.is_cown())
    {
      return {ValueType::Cown, &ffi_type_pointer};
    }
    else if (type_id.is_complex())
    {
      auto idx = type_id.complex();
      assert(idx < complex_types.size());
      return layout_complex_type(complex_types.at(idx));
    }

    // Dyn, ref.
    return {ValueType::Invalid, &ffi_type_value};
  }

  std::pair<ValueType, ffi_type*> Program::layout_complex_type(ComplexType& t)
  {
    switch (t.tag)
    {
      case ComplexType::Tag::Base:
        return layout_type_id(TypeId::leb(t.type_id));

      case ComplexType::Tag::Ref:
        return {ValueType::Invalid, &ffi_type_pointer};

      case ComplexType::Tag::Array:
        return {ValueType::Array, &ffi_type_pointer};

      case ComplexType::Tag::Cown:
        return {ValueType::Cown, &ffi_type_pointer};

      case ComplexType::Tag::Union:
      {
        // If all elements of the union have the same representation, use it.
        // There are no unions of unions, so can examine just the next typeid.
        auto& child = t.children.at(0);
        auto rep = layout_complex_type(child);
        bool ok = true;

        for (size_t i = 0; i < t.children.size(); i++)
        {
          if (rep != layout_complex_type(t.children.at(i)))
          {
            ok = false;
            break;
          }
        }

        if (ok)
          return rep;

        return {ValueType::Invalid, &ffi_type_value};
      }
    }
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
      return std::format("class {}", cls.type_id.cls());

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
    auto type_u8 = TypeId::val(ValueType::U8);
    auto arg_rep = layout_type_id(type_u8);

    auto type_argv = TypeId::argv();
    auto argv_rep = layout_type_id(type_argv);

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

    // Primitive classes.
    primitives.resize(NumPrimitiveClasses);

    for (auto& cls : primitives)
    {
      cls.type_id = TypeId::dyn();
      cls.debug_info = size_t(-1);

      if (!parse_methods(cls, pc))
        return false;
    }

    // User-defined classes.
    auto num_classes = uleb(pc);
    classes.resize(num_classes);
    size_t class_id = 0;

    for (auto& cls : classes)
    {
      cls.type_id = TypeId::cls(class_id++);
      cls.debug_info = uleb(pc);

      if (!parse_fields(cls, pc))
        return false;

      if (!parse_methods(cls, pc))
        return false;
    }

    // FFI information.
    auto num_libs = uleb(pc);
    libs.reserve(num_libs);
    for (size_t i = 0; i < num_libs; i++)
      libs.emplace_back(strings.at(uleb(pc)));

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
        symbol.param(TypeId::leb(uleb(pc)));

      symbol.ret(TypeId::leb(uleb(pc)));
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

    if (!(functions.at(MainFuncId).return_type < TypeId::val(ValueType::I32)))
    {
      logging::Error() << file << ": `main` must return i32" << std::endl;
      return false;
    }

    // Complex types.
    auto num_types = uleb(pc);
    complex_types.resize(num_types);

    for (auto& t : complex_types)
    {
      auto pt = &t;

      if (!parse_complex_type(pt, pc))
        return false;
    }

    // Prepare symbols now that all type information is available.
    for (auto& symbol : symbols)
    {
      if (!symbol.prepare())
      {
        logging::Error() << file << ": couldn't prepare symbol" << std::endl;
        return false;
      }
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
      string_table(pc, di_strings);
      di = pc;
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
      f.param_types.at(i) = TypeId::leb(uleb(pc));

    f.return_type = TypeId::leb(uleb(pc));

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
    auto num_fields = uleb(pc);
    cls.field_map.reserve(num_fields);
    cls.fields.resize(num_fields);

    for (size_t i = 0; i < num_fields; i++)
    {
      auto& f = cls.fields.at(i);
      cls.field_map.emplace(uleb(pc), i);
      f.type_id = TypeId::leb(uleb(pc));
    }

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

    // Calculate the class size.
    if (!cls.calc_size())
    {
      logging::Error() << file << ": couldn't calculate class size"
                       << std::endl;
      return false;
    }

    return true;
  }

  bool Program::parse_complex_type(ComplexType*& t, PC& pc)
  {
    auto nest = [](ComplexType*& t, auto tag) {
      if (t->depth == 0)
      {
        t->tag = tag;
        t->depth = 1;
      }
      else if (t->tag == tag)
      {
        t->depth++;
      }
      else
      {
        t->children.resize(1);
        t = &t->children.at(0);
        t->tag = tag;
        t->depth = 1;
      }
    };

    while (true)
    {
      auto type = uleb(pc);

      switch (type)
      {
        case type::Ref:
        {
          nest(t, ComplexType::Tag::Ref);
          break;
        }

        case type::Cown:
        {
          nest(t, ComplexType::Tag::Cown);
          break;
        }

        case type::Array:
        {
          nest(t, ComplexType::Tag::Array);
          break;
        }

        case type::Union:
        {
          nest(t, ComplexType::Tag::Union);
          auto size = uleb(pc);
          t->children.resize(size);

          for (auto& child : t->children)
          {
            auto pchild = &child;

            if (!parse_complex_type(pchild, pc))
              return false;
          }

          return true;
        }

        default:
        {
          // This is dyn, a primitive, or a class.
          if (
            (type != type::Dyn) && !type::is_val(type) && !type::is_class(type))
          {
            logging::Error() << file << ": malformed complex type" << std::endl;
            return false;
          }

          nest(t, ComplexType::Tag::Base);
          t->type_id = type;
          return true;
        }
      }
    }
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
