#include "bytecode.h"

#include "lang.h"

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

  void LabelState::resize(size_t size)
  {
    in.resize(size);
    out.resize(size);
    dead.resize(size);
  }

  void LabelState::def(size_t r)
  {
    // We've defined a register, so it's live.
    out.set(r);
    dead.reset(r);
  }

  bool LabelState::use(size_t r)
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
    return true;
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

  State::State()
  {
    primitives.resize(NumPrimitiveClasses);

    // Reserve a function ID for `@main`.
    functions.push_back(FuncState(nullptr));
    func_ids.insert({ST::di().string("@main"), MainFuncId});

    // Reserve method IDs.
    method_ids.insert({ST::di().string("@final"), FinalMethodId});
    method_ids.insert({ST::di().string("@apply"), ApplyMethodId});
  }

  std::optional<size_t> State::get_type_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = type_ids.find(name);

    if (find == type_ids.end())
      return {};

    return find->second;
  }

  bool State::add_type(Node type)
  {
    auto name = ST::di().string(type / TypeId);
    auto find = type_ids.find(name);

    if (find != type_ids.end())
      return false;

    type_ids.insert({name, type_ids.size()});
    typedefs.push_back(type);
    return true;
  }

  std::optional<size_t> State::get_class_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = class_ids.find(name);

    if (find == class_ids.end())
      return {};

    return find->second;
  }

  bool State::add_class(Node cls)
  {
    auto name = ST::di().string(cls / ClassId);
    auto find = class_ids.find(name);

    if (find != class_ids.end())
      return false;

    class_ids.insert({name, class_ids.size()});
    classes.push_back(cls);
    return true;
  }

  std::optional<size_t> State::get_field_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      return {};

    return find->second;
  }

  void State::add_field(Node field)
  {
    auto name = ST::di().string(field / FieldId);
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      field_ids.insert({name, field_ids.size()});
  }

  std::optional<size_t> State::get_method_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      return {};

    return find->second;
  }

  void State::add_method(Node method)
  {
    auto name = ST::di().string(method / MethodId);
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      method_ids.insert({name, method_ids.size()});
  }

  std::optional<size_t> State::get_func_id(Node id)
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

  FuncState& State::get_func(Node id)
  {
    auto name = ST::di().string(id);
    auto find = func_ids.find(name);
    return functions.at(find->second);
  }

  FuncState& State::add_func(Node func)
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

  std::optional<size_t> State::get_symbol_id(Node id)
  {
    auto name = ST::noemit().string(id);
    auto find = symbol_ids.find(name);

    if (find == symbol_ids.end())
      return {};

    return find->second;
  }

  bool State::add_symbol(Node symbol)
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

  std::optional<size_t> State::get_library_id(Node lib)
  {
    auto name = ST::exec().string(lib / String);
    auto find = library_ids.find(name);

    if (find == library_ids.end())
      return {};

    return find->second;
  }

  void State::add_library(Node lib)
  {
    auto name = ST::exec().string(lib / String);
    auto find = library_ids.find(name);

    if (find != library_ids.end())
      return;

    library_ids.insert({name, library_ids.size()});
    libraries.push_back(lib);
  }

  void State::def(Node& id)
  {
    auto& func_state = get_func(id->parent(Func) / FunctionId);
    auto& label_state = func_state.get_label(id->parent(Label) / LabelId);
    label_state.def(*func_state.get_register_id(id));
  }

  bool State::use(Node& id)
  {
    auto& func_state = get_func(id->parent(Func) / FunctionId);
    auto& label_state = func_state.get_label(id->parent(Label) / LabelId);

    if (!label_state.use(*func_state.get_register_id(id)))
    {
      error = true;
      id->parent()->replace(id, err(clone(id), "use of dead register"));
      return false;
    }

    return true;
  }

  bool State::kill(Node& id)
  {
    auto& func_state = get_func(id->parent(Func) / FunctionId);
    auto& label_state = func_state.get_label(id->parent(Label) / LabelId);

    if (!label_state.kill(*func_state.get_register_id(id)))
    {
      error = true;
      id->parent()->replace(id, err(clone(id), "use of dead register"));
      return false;
    }

    return true;
  }

  void State::gen()
  {
    std::vector<uint8_t> hdr;
    std::vector<uint8_t> di;
    std::vector<uint8_t> code;

    std::function<size_t(Node)> typ = [&](Node type) -> size_t {
      if (type == Dyn)
        return type::dyn();

      if (type == ClassId)
        return type::cls(*get_class_id(type));

      if (type == TypeId)
        return type::def(classes.size(), *get_type_id(type));

      if (type == Array)
        return type::array(typ(type / Type));

      if (type == Ref)
        return type::ref(typ(type / Type));

      if (type == Cown)
        return type::cown(typ(type / Type));

      return type::val(val(type));
    };

    hdr << uleb(MagicNumber);
    hdr << uleb(CurrentVersion);

    // Add the compilation path to the string table.
    auto comp_path = ST::di().string(options().compilation_path.string());

    // Debug info string table.
    di << uleb(ST::di().size());

    for (size_t i = 0; i < ST::di().size(); i++)
      di << ST::di().at(i);

    // The compilation path.
    di << uleb(comp_path);

    // Exec string table.
    hdr << uleb(ST::exec().size());

    for (size_t i = 0; i < ST::exec().size(); i++)
      hdr << ST::exec().at(i);

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
          << uleb((symbol / Vararg) == Vararg)
          << uleb((symbol / FFIParams)->size());

      for (auto& param : *(symbol / FFIParams))
        hdr << uleb(typ(param));

      hdr << uleb(typ(symbol / Return));
    }

    // Typedefs.
    hdr << uleb(typedefs.size());

    for (auto& type : typedefs)
    {
      hdr << uleb((type / Union)->size());

      for (auto& def : *(type / Union))
        hdr << uleb(typ(def));
    }

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
    hdr << uleb(classes.size());

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

      auto cls = [&](Node stmt) { return uleb(*get_class_id(stmt / ClassId)); };

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

      auto adv_di = [&]() {
        auto di_cur_pc = code.size();

        if (di_cur_pc > di_last_pc)
        {
          di << d(DIOp::Skip, di_cur_pc - di_last_pc);
          di_last_pc = di_cur_pc;
        }
      };

      auto stmt_di = [&](Node stmt) {
        // Use the source and offset in the AST.
        auto file = ST::di().file(stmt->location().source->origin());
        auto pos = stmt->location().pos;

        if (file != di_file)
        {
          adv_di();
          di << d(DIOp::File, file);
          di_file = file;
          di_offset = 0;
        }

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

        for (auto stmt : *(label / Body))
        {
          if (stmt == Source)
          {
            adv_di();
            di_file = ST::di().file(stmt / String);
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
                code << uleb(true);
              else
                code << uleb(false);
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
          else if (stmt == FieldRef)
          {
            auto arg = stmt / Arg;

            if (arg == ArgMove)
              code << uleb(+Op::RefMove);
            else
              code << uleb(+Op::RefCopy);

            code << dst(stmt) << src(arg) << fld(stmt);
          }
          else if (stmt == ArrayRef)
          {
            auto arg = stmt / Arg;

            if (arg == ArgMove)
              code << uleb(+Op::ArrayRefMove);
            else
              code << uleb(+Op::ArrayRefCopy);

            code << dst(stmt) << src(arg) << rhs(stmt);
          }
          else if (stmt == ArrayRefConst)
          {
            auto arg = stmt / Arg;

            if (arg == ArgMove)
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

            if (arg == ArgMove)
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
            onearg(stmt / Arg);
            args(stmt / Args);
            code << uleb(+Op::When) << dst(stmt);
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

        auto term = label / Return;

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

    // Length of the debug info section.
    if (options().strip)
      hdr << uleb(0);
    else
      hdr << uleb(di.size());

    std::ofstream f(options().bytecode_file, std::ios::binary | std::ios::out);
    f.write(reinterpret_cast<const char*>(hdr.data()), hdr.size());

    if (!options().strip)
      f.write(reinterpret_cast<const char*>(di.data()), di.size());

    f.write(reinterpret_cast<const char*>(code.data()), code.size());

    if (!f)
    {
      logging::Error() << "Error writing to: " << options().bytecode_file
                       << std::endl;
    }
  }
}
