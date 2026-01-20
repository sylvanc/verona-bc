#include "bytecode.h"

#include "lang.h"

#include <zstd.h>

namespace vbcc
{
  using namespace vbci;

  template<typename T>
  struct sleb
  {
    T value;
    sleb(T value) : value(value) {}
  };

  template<typename T>
  struct uleb
  {
    T value;
    uleb(T value) : value(value) {}
  };

  template<typename T>
  struct d
  {
    DIOp op;
    T value;
    d(DIOp op, T value) : op(op), value(value) {}
  };

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, sleb<T>&& s)
  {
    // This uses zigzag encoding.
    auto value = (s.value << 1) ^ (s.value >> ((sizeof(T) * 8) - 1));
    return b << uleb(value);
  }

  template<>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, sleb<float>&& s)
  {
    auto value = std::bit_cast<int32_t>(s.value);
    return b << sleb(value);
  }

  template<>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, sleb<double>&& s)
  {
    auto value = std::bit_cast<int64_t>(s.value);
    return b << sleb(value);
  }

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, uleb<T>&& u)
  {
    auto value = u.value;

    while (value > 0x7F)
    {
      b.push_back((value & 0x7F) | 0x80);
      value >>= 7;
    }

    b.push_back(value);
    return b;
  }

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, d<T>&& d)
  {
    auto value = (d.value << 2) | +d.op;
    return b << uleb(value);
  }

  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, const std::string& str)
  {
    b << uleb(str.size());
    b.insert(b.end(), str.begin(), str.end());
    return b;
  }

  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, const std::string_view& str)
  {
    b << uleb(str.size());
    b.insert(b.end(), str.begin(), str.end());
    return b;
  }

  uleb<size_t> rgn(Node node)
  {
    auto region = node / Region;

    if (region == RegionRC)
      return +RegionType::RegionRC;
    else if (region == RegionGC)
      return +RegionType::RegionGC;
    else if (region == RegionArena)
      return +RegionType::RegionArena;

    assert(false);
    return size_t(-1);
  }

  template<typename T>
  T lit(Node node)
  {
    auto view = node->location().view();
    auto first = view.data();
    auto last = first + view.size();
    T t = 0;

    if (node == Bin)
      std::from_chars(first + 2, last, t, 2);
    else if (node == Oct)
      std::from_chars(first + 2, last, t, 8);
    else if (node == Hex)
      std::from_chars(first + 2, last, t, 16);
    else if (node == Int)
      std::from_chars(first, last, t, 10);

    return t;
  }

  template<>
  float lit<float>(Node node)
  {
    auto view = node->location().view();
    auto first = view.data();
    auto last = first + view.size();
    float t = 0;

    if (node == Float)
      std::from_chars(first, last, t);
    else if (node == HexFloat)
      std::from_chars(first + 2, last, t);

    return t;
  }

  template<>
  double lit<double>(Node node)
  {
    auto view = node->location().view();
    auto first = view.data();
    auto last = first + view.size();
    double t = 0;

    if (node == Float)
      std::from_chars(first, last, t);
    else if (node == HexFloat)
      std::from_chars(first + 2, last, t);

    return t;
  }

  size_t VecHash::operator()(const std::vector<uint8_t>& v) const noexcept
  {
    auto h = size_t(14695981039346656037ull);

    for (auto b : v)
      h = (h ^ b) * size_t(1099511628211ull);

    return h;
  }

  void LabelState::resize(size_t size)
  {
    first_def.resize(size);
    first_use.resize(size);
    last_use.resize(size);
    in.resize(size);
    defd.resize(size);
    dead.resize(size);
    out.resize(size);
  }

  std::pair<bool, std::string> LabelState::def(size_t r, Node& node, bool var)
  {
    // Not a var, and has alrady been defined, should not be able to re-define
    if (!var && defd.test(r))
      return {false, "redefinition of register"};

    defd.set(r);

    // Not a var, is in the in set (which means it has been used but not
    // defined) then this is a use before def error
    if (!var && in.test(r))
      return {false, "use before def"};

    if (out.test(r))
    {
      // Assigning to a non-variable used register is an error.
      if (!var)
        return {false, "redefinition of register"};

      automove(r);
    }
    else
    {
      out.set(r);
      dead.reset(r);
    }

    if (!first_def.at(r))
      first_def[r] = node;

    if (!first_use.at(r))
      first_use[r] = node;

    last_use[r] = {};
    return {true, ""};
  }

  bool LabelState::use(size_t r, Node& node)
  {
    // We've used a register. If it's not live, we require it and set it as
    // live.
    if (dead.test(r))
      return false;

    if (!out.test(r))
    {
      out.set(r);
      in.set(r);
    }

    if (!first_use.at(r))
      first_use[r] = node;

    last_use[r] = node;
    return true;
  }

  bool LabelState::kill(size_t r)
  {
    // We've killed a register. If it's live, we kill it. If it's not live, we
    // require it.
    if (dead.test(r))
      return false;

    if (out.test(r))
      out.reset(r);
    else
      in.set(r);

    dead.set(r);
    last_use[r] = {};
    return true;
  }

  void LabelState::automove(size_t r)
  {
    auto n = last_use.at(r);

    if (!n)
      return;

    last_use[r] = {};
    auto parent = n->parent();

    if ((parent == Arg) && (parent->front() == ArgCopy))
      parent / Type = ArgMove;
    else if (parent == Copy)
      parent->parent()->replace(parent, Move << *parent);
  }

  std::optional<size_t> FuncState::get_label_id(Node id)
  {
    auto index = ST::noemit().string(id->location().view());
    auto find = label_idxs.find(index);

    if (find == label_idxs.end())
      return {};

    return find->second;
  }

  LabelState& FuncState::get_label(Node id)
  {
    auto index = ST::noemit().string(id->location().view());
    auto find = label_idxs.find(index);
    return labels.at(find->second);
  }

  bool FuncState::add_label(Node id)
  {
    auto index = ST::noemit().string(id->location().view());
    auto find = label_idxs.find(index);

    if (find != label_idxs.end())
      return false;

    label_idxs.insert({index, label_idxs.size()});
    labels.emplace_back();
    return true;
  }

  std::optional<size_t> FuncState::get_register_id(Node id)
  {
    auto index = ST::di().string(id);
    auto find = register_idxs.find(index);

    if (find == register_idxs.end())
      return {};

    return find->second;
  }

  bool FuncState::add_register(Node id)
  {
    auto index = ST::di().string(id);
    auto find = register_idxs.find(index);

    if (find != register_idxs.end())
      return false;

    register_idxs.insert({index, register_idxs.size()});
    register_names.push_back(index);
    assert(register_idxs.size() == register_names.size());
    return true;
  }

  Bytecode::Bytecode()
  {
    primitives.resize(NumPrimitiveClasses);

    // Reserve a function ID for `@main`.
    auto main_name = ST::di().string("@main");
    auto func_main = FuncState(nullptr);
    func_main.name = main_name;
    functions.push_back(func_main);
    func_ids.insert({main_name, MainFuncId});

    // Reserve method IDs.
    method_ids.insert({ST::di().string("@final"), FinalMethodId});
    method_ids.insert({ST::di().string("@apply"), ApplyMethodId});
  }

  void Bytecode::add_path(const std::filesystem::path& path)
  {
    auto full = std::filesystem::canonical(path);

    if (!std::filesystem::is_directory(full))
      full = full.parent_path();

    source_paths.push_back(full);
  }

  std::optional<size_t> Bytecode::get_typealias_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = type_ids.find(name);

    if (find == type_ids.end())
      return {};

    return find->second;
  }

  Node Bytecode::get_typealias(Node id)
  {
    auto name = ST::di().string(id);
    auto find = type_ids.find(name);
    return typealiases.at(find->second);
  }

  bool Bytecode::add_typealias(Node type)
  {
    auto name = ST::di().string(type / TypeId);
    auto find = type_ids.find(name);

    if (find != type_ids.end())
      return false;

    type_ids.insert({name, type_ids.size()});
    typealiases.push_back(type);
    return true;
  }

  std::optional<size_t> Bytecode::get_class_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = class_ids.find(name);

    if (find == class_ids.end())
      return {};

    return find->second;
  }

  bool Bytecode::add_class(Node cls)
  {
    auto name = ST::di().string(cls / ClassId);
    auto find = class_ids.find(name);

    if (find != class_ids.end())
      return false;

    class_ids.insert({name, class_ids.size()});
    classes.push_back(cls);
    return true;
  }

  std::optional<size_t> Bytecode::get_field_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      return {};

    return find->second;
  }

  void Bytecode::add_field(Node field)
  {
    auto name = ST::di().string(field / FieldId);
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      field_ids.insert({name, field_ids.size()});
  }

  std::optional<size_t> Bytecode::get_method_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      return {};

    return find->second;
  }

  void Bytecode::add_method(Node method)
  {
    auto name = ST::di().string(method / MethodId);
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      method_ids.insert({name, method_ids.size()});
  }

  std::optional<size_t> Bytecode::get_func_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = func_ids.find(name);

    if (find == func_ids.end())
      return {};

    // Pretend not to have an id if the function name is reserved.
    auto func_id = find->second;

    if (!functions.at(func_id).func)
      return {};

    return func_id;
  }

  FuncState& Bytecode::get_func(Node id)
  {
    auto name = ST::di().string(id);
    auto find = func_ids.find(name);
    return functions.at(find->second);
  }

  FuncState& Bytecode::add_func(Node func)
  {
    auto name = ST::di().string(func / FunctionId);
    auto find = func_ids.find(name);
    size_t func_id;

    if (find == func_ids.end())
    {
      // This is a fresh func_id.
      func_id = func_ids.size();
      func_ids.insert({name, func_id});
      functions.push_back(func);
    }
    else
    {
      // This is a reserved func_id.
      func_id = find->second;
      functions.at(func_id).func = func;
    }

    auto& func_state = functions.at(func_id);
    func_state.name = name;
    func_state.params = (func / Params)->size();
    return func_state;
  }

  std::optional<size_t> Bytecode::get_symbol_id(Node id)
  {
    auto name = ST::noemit().string(id);
    auto find = symbol_ids.find(name);

    if (find == symbol_ids.end())
      return {};

    return find->second;
  }

  bool Bytecode::add_symbol(Node symbol)
  {
    auto name = ST::noemit().string(symbol / SymbolId);
    auto find = symbol_ids.find(name);

    if (find != symbol_ids.end())
      return false;

    ST::exec().string(symbol / Lhs);
    ST::exec().string(symbol / Rhs);
    symbol_ids.insert({name, symbol_ids.size()});
    symbols.push_back(symbol);
    return true;
  }

  std::optional<size_t> Bytecode::get_library_id(Node lib)
  {
    auto name = ST::exec().string(lib / String);
    auto find = library_ids.find(name);

    if (find == library_ids.end())
      return {};

    return find->second;
  }

  void Bytecode::add_library(Node lib)
  {
    auto name = ST::exec().string(lib / String);
    auto find = library_ids.find(name);

    if (find != library_ids.end())
      return;

    library_ids.insert({name, library_ids.size()});
    libraries.push_back(lib);
  }

  void Bytecode::gen(std::filesystem::path output, bool strip)
  {
    wf::push_back(wfIR);

    if (output.empty())
      output = "out.vbc";

    std::vector<uint8_t> hdr;
    std::vector<uint8_t> di;
    std::vector<uint8_t> code;
    std::map<ST::Index, trieste::Source> di_source;

    hdr << uleb(MagicNumber);
    hdr << uleb(CurrentVersion);

    // Exec string table.
    hdr << uleb(ST::exec().size());

    for (size_t i = 0; i < ST::exec().size(); i++)
      hdr << ST::exec().at(i);

    // Class and complex primitive count.
    hdr << uleb(classes.size());
    hdr << uleb(complex_primitives.size());

    // Primitive classes.
    for (auto& p : primitives)
    {
      if (p)
      {
        auto methods = p / Methods;
        hdr << uleb(methods->size());

        for (auto& method : *methods)
        {
          hdr << uleb(*get_method_id(method / MethodId))
              << uleb(*get_func_id(method / FunctionId));
        }
      }
      else
      {
        hdr << uleb(0);
      }
    }

    // Classes.
    for (auto& c : classes)
    {
      hdr << uleb(di.size());
      di << uleb(ST::di().string(c / ClassId));

      auto fields = c / Fields;
      hdr << uleb(fields->size());

      for (auto& field : *fields)
      {
        hdr << uleb(*get_field_id(field / FieldId));
        hdr << uleb(typ(field / Type));
        di << uleb(ST::di().string(field / FieldId));
      }

      auto methods = c / Methods;
      hdr << uleb(methods->size());

      for (auto& method : *methods)
      {
        hdr << uleb(*get_method_id(method / MethodId))
            << uleb(*get_func_id(method / FunctionId));
        di << uleb(ST::di().string(method / MethodId));
      }
    }

    // Complex primitive classes.
    for (auto& p : complex_primitives)
    {
      if (p)
      {
        auto methods = p / Methods;
        hdr << uleb(methods->size());

        for (auto& method : *methods)
        {
          hdr << uleb(*get_method_id(method / MethodId))
              << uleb(*get_func_id(method / FunctionId));
        }
      }
      else
      {
        hdr << uleb(0);
      }
    }

    // FFI libraries.
    hdr << uleb(libraries.size());

    for (auto& lib : libraries)
      hdr << uleb(ST::exec().string(lib / String));

    hdr << uleb(symbols.size());

    for (auto& symbol : symbols)
    {
      hdr << uleb(*get_library_id(symbol->parent(Lib)))
          << uleb(ST::exec().string(symbol / Lhs))
          << uleb(ST::exec().string(symbol / Rhs))
          << uleb(((symbol / Vararg) == Vararg) ? 1 : 0)
          << uleb((symbol / FFIParams)->size());

      for (auto& param : *(symbol / FFIParams))
        hdr << uleb(typ(param));

      hdr << uleb(typ(symbol / Return));
    }

    // Functions.
    hdr << uleb(functions.size());

    for (auto& func_state : functions)
    {
      hdr << uleb(func_state.register_idxs.size());
      hdr << uleb(di.size());

      // Parameter and return types.
      hdr << uleb(func_state.params);

      for (auto& param : *(func_state.func / Params))
        hdr << uleb(typ(param / Type));

      hdr << uleb(typ(func_state.func / Type));

      // Labels.
      hdr << uleb(func_state.label_idxs.size());

      // Function name.
      di << uleb(func_state.name);

      // Register names.
      for (auto& name : func_state.register_names)
        di << uleb(name);

      auto dst = [&](Node stmt) {
        return uleb(*func_state.get_register_id(stmt / LocalId));
      };

      auto lhs = [&](Node stmt) {
        return uleb(*func_state.get_register_id(stmt / Lhs));
      };

      auto rhs = [&](Node stmt) {
        return uleb(*func_state.get_register_id(stmt / Rhs));
      };

      auto src = rhs;

      auto cls = [&](Node stmt) { return uleb(typ(stmt / ClassId)); };

      auto fld = [&](Node stmt) { return uleb(*get_field_id(stmt / FieldId)); };

      auto mth = [&](Node stmt) {
        return uleb(*get_method_id(stmt / MethodId));
      };

      auto fn = [&](Node stmt) {
        return uleb(*get_func_id(stmt / FunctionId));
      };

      auto onearg = [&](Node arg) {
        if ((arg / Type) == ArgMove)
          code << uleb(+Op::ArgMove) << uleb(src(arg));
        else
          code << uleb(+Op::ArgCopy) << uleb(src(arg));
      };

      auto args = [&](Node args) {
        for (auto arg : *args)
          onearg(arg);
      };

      constexpr size_t no_value = size_t(-1);
      size_t di_file = no_value;
      size_t di_offset = 0;
      size_t di_last_pc = code.size();
      bool explicit_di = false;

      // Keep track of all included source files.
      auto di_source_curr = di_source.end();

      auto adv_di = [&]() {
        auto di_cur_pc = code.size();

        if (di_cur_pc > di_last_pc)
        {
          di << d(DIOp::Skip, di_cur_pc - di_last_pc);
          di_last_pc = di_cur_pc;
        }
      };

      auto stmt_di = [&](Node& stmt) {
        // Record nothing for empty or synthetic source locations.
        if (
          !stmt->location().source || stmt->location().source->origin().empty())
          return;

        // Use the source and offset in the AST.
        if (
          (di_source_curr == di_source.end()) ||
          (di_source_curr->second != stmt->location().source))
        {
          // Pick a non-relative path.
          std::filesystem::path rel_path;

          for (auto& path : source_paths)
          {
            rel_path = std::filesystem::relative(
              stmt->location().source->origin(), path);

            if (!rel_path.empty() && (rel_path.c_str()[0] != '.'))
              break;
          }

          if (rel_path.empty() || (rel_path.c_str()[0] == '.'))
            rel_path = stmt->location().source->origin();

          di_source_curr =
            di_source
              .emplace(
                ST::di().string(rel_path.string()), stmt->location().source)
              .first;

          adv_di();
          di << d(DIOp::File, di_source_curr->first);
          di_file = di_source_curr->first;
          di_offset = 0;
        }

        auto pos = stmt->location().pos;

        if (pos != di_offset)
        {
          // Offset will also advance the PC by one, so reduce any Skip by one.
          di_last_pc++;
          adv_di();
          di << d(DIOp::Offset, pos - di_offset);
          di_offset = pos;
        }
      };

      for (auto label : *(func_state.func / Labels))
      {
        // Save the pc for this label.
        hdr << uleb(code.size());

        for (Node stmt : *(label / Body))
        {
          if (stmt == Source)
          {
            adv_di();
            di_file = ST::di().string(stmt / String);
            di_offset = 0;
            explicit_di = true;
            di << d(DIOp::File, di_file);
            continue;
          }
          else if (stmt == Offset)
          {
            adv_di();
            di_offset = lit<size_t>(stmt / Int);
            explicit_di = true;
            di << d(DIOp::Offset, di_offset);
            continue;
          }
          else if (!explicit_di)
          {
            stmt_di(stmt);
          }

          if (stmt == Const)
          {
            auto t = stmt / Type;
            auto v = stmt / Rhs;

            if (t == None)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t));
            }
            else if (t == Bool)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t));

              if ((stmt / Rhs) == True)
                code << uleb(1);
              else
                code << uleb(0);
            }
            else if (t == I8)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << sleb(lit<int8_t>(v));
            }
            else if (t == U8)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << uleb(lit<uint8_t>(v));
            }
            else if (t == I16)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << sleb(lit<int16_t>(v));
            }
            else if (t == U16)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << uleb(lit<uint16_t>(v));
            }
            else if (t == I32)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << sleb(lit<int32_t>(v));
            }
            else if (t == U32)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << uleb(lit<uint32_t>(v));
            }
            else if (t->in({I64, ILong, ISize}))
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << sleb(lit<int64_t>(v));
            }
            else if (t->in({U64, ULong, USize, Ptr}))
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << uleb(lit<uint64_t>(v));
            }
            else if (t == F32)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << sleb(lit<float>(v));
            }
            else if (t == F64)
            {
              code << uleb(+Op::Const) << dst(stmt) << uleb(+val(t))
                   << sleb(lit<double>(v));
            }
          }
          else if (stmt == ConstStr)
          {
            code << uleb(+Op::String) << dst(stmt)
                 << uleb(ST::exec().string(stmt / String));
          }
          else if (stmt == Convert)
          {
            code << uleb(+Op::Convert) << dst(stmt) << uleb(+val(stmt / Type))
                 << rhs(stmt);
          }
          else if (stmt == New)
          {
            args(stmt / Args);
            code << uleb(+Op::New) << dst(stmt) << cls(stmt);
          }
          else if (stmt == Stack)
          {
            args(stmt / Args);
            code << uleb(+Op::Stack) << dst(stmt) << cls(stmt);
          }
          else if (stmt == Heap)
          {
            args(stmt / Args);
            code << uleb(+Op::Heap) << dst(stmt) << rhs(stmt) << cls(stmt);
          }
          else if (stmt == Region)
          {
            args(stmt / Args);
            code << uleb(+Op::Region) << dst(stmt) << rgn(stmt) << cls(stmt);
          }
          else if (stmt == NewArray)
          {
            code << uleb(+Op::NewArray) << dst(stmt) << rhs(stmt)
                 << uleb(typ(stmt / Type));
          }
          else if (stmt == NewArrayConst)
          {
            code << uleb(+Op::NewArrayConst) << dst(stmt)
                 << uleb(typ(stmt / Type)) << uleb(lit<uint64_t>(stmt / Rhs));
          }
          else if (stmt == StackArray)
          {
            code << uleb(+Op::StackArray) << dst(stmt) << rhs(stmt)
                 << uleb(typ(stmt / Type));
          }
          else if (stmt == StackArrayConst)
          {
            code << uleb(+Op::StackArrayConst) << dst(stmt)
                 << uleb(typ(stmt / Type)) << uleb(lit<uint64_t>(stmt / Rhs));
          }
          else if (stmt == HeapArray)
          {
            code << uleb(+Op::HeapArray) << dst(stmt) << lhs(stmt) << rhs(stmt)
                 << uleb(typ(stmt / Type));
          }
          else if (stmt == HeapArrayConst)
          {
            code << uleb(+Op::HeapArrayConst) << dst(stmt) << src(stmt)
                 << uleb(typ(stmt / Type)) << uleb(lit<uint64_t>(stmt / Rhs));
          }
          else if (stmt == RegionArray)
          {
            code << uleb(+Op::RegionArray) << dst(stmt) << rgn(stmt)
                 << rhs(stmt) << uleb(typ(stmt / Type));
          }
          else if (stmt == RegionArrayConst)
          {
            code << uleb(+Op::RegionArrayConst) << dst(stmt) << rgn(stmt)
                 << uleb(typ(stmt / Type)) << uleb(lit<uint64_t>(stmt / Rhs));
          }
          else if (stmt == Copy)
          {
            code << uleb(+Op::Copy) << dst(stmt) << src(stmt);
          }
          else if (stmt == Move)
          {
            code << uleb(+Op::Move) << dst(stmt) << src(stmt);
          }
          else if (stmt == Drop)
          {
            code << uleb(+Op::Drop) << dst(stmt);
          }
          else if (stmt == RegisterRef)
          {
            code << uleb(+Op::RegisterRef) << dst(stmt) << src(stmt);
          }
          else if (stmt == FieldRef)
          {
            auto arg = stmt / Arg;

            if ((arg / Type) == ArgMove)
              code << uleb(+Op::FieldRefMove);
            else
              code << uleb(+Op::FieldRefCopy);

            code << dst(stmt) << src(arg) << fld(stmt);
          }
          else if (stmt == ArrayRef)
          {
            auto arg = stmt / Arg;

            if ((arg / Type) == ArgMove)
              code << uleb(+Op::ArrayRefMove);
            else
              code << uleb(+Op::ArrayRefCopy);

            code << dst(stmt) << src(arg) << rhs(stmt);
          }
          else if (stmt == ArrayRefConst)
          {
            auto arg = stmt / Arg;

            if ((arg / Type) == ArgMove)
              code << uleb(+Op::ArrayRefMoveConst);
            else
              code << uleb(+Op::ArrayRefCopyConst);

            code << dst(stmt) << src(arg) << uleb(lit<uint64_t>(stmt / Rhs));
          }
          else if (stmt == Load)
          {
            code << uleb(+Op::Load) << dst(stmt) << rhs(stmt);
          }
          else if (stmt == Store)
          {
            auto arg = stmt / Arg;

            if ((arg / Type) == ArgMove)
              code << uleb(+Op::StoreMove);
            else
              code << uleb(+Op::StoreCopy);

            code << dst(stmt) << src(stmt) << src(arg);
          }
          else if (stmt == FnPointer)
          {
            auto f = stmt / Rhs;

            if (f == FunctionId)
              code << uleb(+Op::LookupStatic) << dst(stmt)
                   << uleb(*get_func_id(f));
            else
              code << uleb(+Op::LookupFFI) << dst(stmt)
                   << uleb(*get_symbol_id(f));
          }
          else if (stmt == Lookup)
          {
            code << uleb(+Op::LookupDynamic) << dst(stmt) << src(stmt)
                 << mth(stmt);
          }
          else if (stmt == Call)
          {
            args(stmt / Args);
            code << uleb(+Op::CallStatic) << dst(stmt) << fn(stmt);
          }
          else if (stmt == CallDyn)
          {
            args(stmt / Args);
            code << uleb(+Op::CallDynamic) << dst(stmt) << src(stmt);
          }
          else if (stmt == Subcall)
          {
            args(stmt / Args);
            code << uleb(+Op::SubcallStatic) << dst(stmt) << fn(stmt);
          }
          else if (stmt == SubcallDyn)
          {
            args(stmt / Args);
            code << uleb(+Op::SubcallDynamic) << dst(stmt) << src(stmt);
          }
          else if (stmt == Try)
          {
            args(stmt / Args);
            code << uleb(+Op::TryStatic) << dst(stmt) << fn(stmt);
          }
          else if (stmt == TryDyn)
          {
            args(stmt / Args);
            code << uleb(+Op::TryDynamic) << dst(stmt) << src(stmt);
          }
          else if (stmt == FFI)
          {
            args(stmt / Args);
            code << uleb(+Op::FFI) << dst(stmt)
                 << uleb(*get_symbol_id(stmt / SymbolId));
          }
          else if (stmt == When)
          {
            args(stmt / Args);
            code << uleb(+Op::WhenStatic) << dst(stmt) << uleb(typ(stmt / Cown))
                 << fn(stmt);
          }
          else if (stmt == WhenDyn)
          {
            args(stmt / Args);
            code << uleb(+Op::WhenDynamic) << dst(stmt)
                 << uleb(typ(stmt / Cown)) << src(stmt);
          }
          else if (stmt == Typetest)
          {
            code << uleb(+Op::Typetest) << dst(stmt) << src(stmt)
                 << uleb(typ(stmt / Type));
          }
          else if (stmt == Add)
          {
            code << uleb(+Op::Add) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Sub)
          {
            code << uleb(+Op::Sub) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Mul)
          {
            code << uleb(+Op::Mul) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Div)
          {
            code << uleb(+Op::Div) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Mod)
          {
            code << uleb(+Op::Mod) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Pow)
          {
            code << uleb(+Op::Pow) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == And)
          {
            code << uleb(+Op::And) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Or)
          {
            code << uleb(+Op::Or) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Xor)
          {
            code << uleb(+Op::Xor) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Shl)
          {
            code << uleb(+Op::Shl) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Shr)
          {
            code << uleb(+Op::Shr) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Eq)
          {
            code << uleb(+Op::Eq) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Ne)
          {
            code << uleb(+Op::Ne) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Lt)
          {
            code << uleb(+Op::Lt) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Le)
          {
            code << uleb(+Op::Le) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Gt)
          {
            code << uleb(+Op::Gt) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Ge)
          {
            code << uleb(+Op::Ge) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Min)
          {
            code << uleb(+Op::Min) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Max)
          {
            code << uleb(+Op::Max) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == LogBase)
          {
            code << uleb(+Op::LogBase) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Atan2)
          {
            code << uleb(+Op::Atan2) << dst(stmt) << lhs(stmt) << rhs(stmt);
          }
          else if (stmt == Neg)
          {
            code << uleb(+Op::Neg) << dst(stmt) << src(stmt);
          }
          else if (stmt == Not)
          {
            code << uleb(+Op::Not) << dst(stmt) << src(stmt);
          }
          else if (stmt == Abs)
          {
            code << uleb(+Op::Abs) << dst(stmt) << src(stmt);
          }
          else if (stmt == Ceil)
          {
            code << uleb(+Op::Ceil) << dst(stmt) << src(stmt);
          }
          else if (stmt == Floor)
          {
            code << uleb(+Op::Floor) << dst(stmt) << src(stmt);
          }
          else if (stmt == Exp)
          {
            code << uleb(+Op::Exp) << dst(stmt) << src(stmt);
          }
          else if (stmt == Log)
          {
            code << uleb(+Op::Log) << dst(stmt) << src(stmt);
          }
          else if (stmt == Sqrt)
          {
            code << uleb(+Op::Sqrt) << dst(stmt) << src(stmt);
          }
          else if (stmt == Cbrt)
          {
            code << uleb(+Op::Cbrt) << dst(stmt) << src(stmt);
          }
          else if (stmt == IsInf)
          {
            code << uleb(+Op::IsInf) << dst(stmt) << src(stmt);
          }
          else if (stmt == IsNaN)
          {
            code << uleb(+Op::IsNaN) << dst(stmt) << src(stmt);
          }
          else if (stmt == Sin)
          {
            code << uleb(+Op::Sin) << dst(stmt) << src(stmt);
          }
          else if (stmt == Cos)
          {
            code << uleb(+Op::Cos) << dst(stmt) << src(stmt);
          }
          else if (stmt == Tan)
          {
            code << uleb(+Op::Tan) << dst(stmt) << src(stmt);
          }
          else if (stmt == Asin)
          {
            code << uleb(+Op::Asin) << dst(stmt) << src(stmt);
          }
          else if (stmt == Acos)
          {
            code << uleb(+Op::Acos) << dst(stmt) << src(stmt);
          }
          else if (stmt == Atan)
          {
            code << uleb(+Op::Atan) << dst(stmt) << src(stmt);
          }
          else if (stmt == Sinh)
          {
            code << uleb(+Op::Sinh) << dst(stmt) << src(stmt);
          }
          else if (stmt == Cosh)
          {
            code << uleb(+Op::Cosh) << dst(stmt) << src(stmt);
          }
          else if (stmt == Tanh)
          {
            code << uleb(+Op::Tanh) << dst(stmt) << src(stmt);
          }
          else if (stmt == Asinh)
          {
            code << uleb(+Op::Asinh) << dst(stmt) << src(stmt);
          }
          else if (stmt == Acosh)
          {
            code << uleb(+Op::Acosh) << dst(stmt) << src(stmt);
          }
          else if (stmt == Atanh)
          {
            code << uleb(+Op::Atanh) << dst(stmt) << src(stmt);
          }
          else if (stmt == Bits)
          {
            code << uleb(+Op::Bits) << dst(stmt) << src(stmt);
          }
          else if (stmt == Len)
          {
            code << uleb(+Op::Len) << dst(stmt) << src(stmt);
          }
          else if (stmt == MakePtr)
          {
            code << uleb(+Op::Ptr) << dst(stmt) << src(stmt);
          }
          else if (stmt == Read)
          {
            code << uleb(+Op::Read) << dst(stmt) << src(stmt);
          }
          else if (stmt == Const_E)
          {
            code << uleb(+Op::Const_E) << dst(stmt);
          }
          else if (stmt == Const_Pi)
          {
            code << uleb(+Op::Const_Pi) << dst(stmt);
          }
          else if (stmt == Const_Inf)
          {
            code << uleb(+Op::Const_Inf) << dst(stmt);
          }
          else if (stmt == Const_NaN)
          {
            code << uleb(+Op::Const_NaN) << dst(stmt);
          }
        }

        Node term = label / Return;

        if (explicit_di)
          adv_di();
        else
          stmt_di(term);

        if (term == Tailcall)
        {
          args(term / MoveArgs);
          code << uleb(+Op::TailcallStatic) << fn(term);
        }
        else if (term == TailcallDyn)
        {
          args(term / MoveArgs);
          code << uleb(+Op::TailcallDynamic) << dst(term);
        }
        else if (term == Return)
        {
          code << uleb(+Op::Return) << dst(term);
        }
        else if (term == Raise)
        {
          code << uleb(+Op::Raise) << dst(term);
        }
        else if (term == Throw)
        {
          code << uleb(+Op::Throw) << dst(term);
        }
        else if (term == Cond)
        {
          auto t = *func_state.get_label_id(term / Lhs);
          auto f = *func_state.get_label_id(term / Rhs);
          code << uleb(+Op::Cond) << dst(term) << uleb(t) << uleb(f);
        }
        else if (term == Jump)
        {
          code << uleb(+Op::Jump)
               << uleb(*func_state.get_label_id(term / LabelId));
        }
      }
    }

    // Types.
    hdr << uleb(types.size());

    for (auto& type : types)
      hdr.insert(hdr.end(), type.begin(), type.end());

    // Code size.
    hdr << uleb(code.size());
    std::ofstream f(output, std::ios::binary | std::ios::out);
    f.write(reinterpret_cast<const char*>(hdr.data()), hdr.size());
    f.write(reinterpret_cast<const char*>(code.data()), code.size());

    if (!strip)
    {
      std::vector<uint8_t> di_strs;

      // Debug info string table.
      di_strs << uleb(ST::di().size());

      for (size_t i = 0; i < ST::di().size(); i++)
        di_strs << ST::di().at(i);

      // Debug info source files.
      di_strs << uleb(di_source.size());

      for (auto& [id, source] : di_source)
      {
        di_strs << uleb(id);
        di_strs << source->view();
      }

      // Debug info ops.
      di_strs.insert(di_strs.end(), di.begin(), di.end());

      // Compress debug info.
      auto cap = ZSTD_compressBound(di_strs.size());
      di.resize(cap);
      auto compressed_size =
        ZSTD_compress(di.data(), cap, di_strs.data(), di_strs.size(), 12);

      if (!ZSTD_isError(compressed_size))
      {
        f.write(reinterpret_cast<const char*>(di.data()), compressed_size);
      }
      else
      {
        logging::Error() << "Error compressing debug info for: " << output
                         << std::endl;
      }
    }

    if (!f)
      logging::Error() << "Error writing to: " << output << std::endl;

    wf::pop_front();
  }

  size_t Bytecode::typ(Node type)
  {
    // If it's a TypeId, encode what it maps to instead.
    if (type == TypeId)
      type = get_typealias(type) / Type;

    if (type == Dyn)
    {
      return DynId;
    }
    else if (type->in(
               {None,
                Bool,
                I8,
                U8,
                I16,
                U16,
                I32,
                U32,
                I64,
                U64,
                ILong,
                ULong,
                ISize,
                USize,
                F32,
                F64,
                Ptr}))
    {
      return +val(type);
    }
    else if (type == ClassId)
    {
      // Class IDs are offset for primitive types.
      return *get_class_id(type) + NumPrimitiveClasses;
    }

    // Encode complex types.
    std::vector<uint8_t> b;

    if (type == Array)
    {
      b << uleb(+TypeTag::Array) << uleb(typ(type / Type));
    }
    else if (type == Cown)
    {
      b << uleb(+TypeTag::Cown) << uleb(typ(type / Type));
    }
    else if (type == Ref)
    {
      b << uleb(+TypeTag::Ref) << uleb(typ(type / Type));
    }
    else if (type == Union)
    {
      b << uleb(+TypeTag::Union);
      b << uleb(type->size());

      std::vector<size_t> child_types;

      for (auto& child : *type)
        child_types.push_back(typ(child));

      std::sort(child_types.begin(), child_types.end());
      child_types.erase(
        std::unique(child_types.begin(), child_types.end()), child_types.end());

      for (auto t : child_types)
        b << uleb(t);
    }

    // Check if we already have this type encoded.
    auto find = type_map.find(b);
    if (find != type_map.end())
      return find->second;

    // Otherwise, add it to the type map. Complex type IDs are offset for
    // primitive types and class IDs.
    auto id = type_map.size() + classes.size() + NumPrimitiveClasses;
    type_map.insert({b, id});
    types.push_back(b);
    return id;
  }
}
