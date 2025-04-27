#include "bytecode.h"

#include "lang.h"

namespace vbcc
{
  using namespace vbci;

  struct e
  {
    Op op;
    uint8_t arg0 = 0;
    uint8_t arg1 = 0;
    uint8_t arg2 = 0;

    operator Code() const
    {
      return Code(op) | (arg0 << 8) | (arg1 << 16) | (arg2 << 24);
    }
  };

  std::vector<Code>& operator<<(std::vector<Code>& code, e&& e)
  {
    code.push_back(e);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, int16_t v)
  {
    code.push_back(v);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, uint16_t v)
  {
    code.push_back(v);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, int32_t v)
  {
    code.push_back(v);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, uint32_t v)
  {
    code.push_back(v);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, int64_t v)
  {
    code.push_back(v & 0xFFFFFFFF);
    code.push_back((v >> 32) & 0xFFFFFFFF);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, uint64_t v)
  {
    code.push_back(v & 0xFFFFFFFF);
    code.push_back((v >> 32) & 0xFFFFFFFF);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, float v)
  {
    auto t = std::bit_cast<uint32_t>(v);
    code.push_back(t);
    return code;
  }

  std::vector<Code>& operator<<(std::vector<Code>& code, double v)
  {
    auto t = std::bit_cast<uint64_t>(v);
    code.push_back(t);
    return code;
  }

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
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& di, uleb<T>&& u)
  {
    auto value = u.value;
    uint8_t byte;

    do
    {
      byte = value & 0x7F;
      value >>= 7;

      if (value)
        byte |= 0x80;

      di.push_back(byte);
    } while (value);

    return di;
  }

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& di, d<T>&& d)
  {
    auto value = (d.value << 2) | +d.op;
    return di << uleb(value);
  }

  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& di, const std::string& str)
  {
    di << uleb(str.size());
    di.insert(di.end(), str.begin(), str.end());
    return di;
  }

  uint8_t rgn(Node node)
  {
    auto region = node / Region;

    if (region == RegionRC)
      return +RegionType::RegionRC;
    if (region == RegionGC)
      return +RegionType::RegionGC;
    if (region == RegionArena)
      return +RegionType::RegionArena;

    return uint8_t(-1);
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

  ArgType argtype(Node arg)
  {
    if ((arg / Type) == ArgMove)
      return ArgType::Move;
    else
      return ArgType::Copy;
  }

  void LabelState::def(uint8_t r)
  {
    // We've defined a register, so it's live.
    out.set(r);
    dead.reset(r);
  }

  bool LabelState::use(uint8_t r)
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

  bool LabelState::kill(uint8_t r)
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

  std::optional<uint8_t> FuncState::get_label_id(Node id)
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

  std::optional<uint8_t> FuncState::get_register_id(Node id)
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

    // Reserve a method ID for `@final`.
    method_ids.insert({ST::di().string("@final"), FinalMethodId});
  }

  std::optional<Id> State::get_type_id(Node id)
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

  std::optional<Id> State::get_class_id(Node id)
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

  std::optional<Id> State::get_field_id(Node id)
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

  std::optional<Id> State::get_method_id(Node id)
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

  std::optional<Id> State::get_func_id(Node id)
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
    Id func_id;

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

  std::optional<Id> State::get_symbol_id(Node id)
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

    ST::ffi().string(symbol / String);
    symbol_ids.insert({name, symbol_ids.size()});
    symbols.push_back(symbol);
    return true;
  }

  std::optional<Id> State::get_library_id(Node lib)
  {
    auto name = ST::ffi().string(lib / String);
    auto find = library_ids.find(name);

    if (find == library_ids.end())
      return {};

    return find->second;
  }

  void State::add_library(Node lib)
  {
    auto name = ST::ffi().string(lib / String);
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
    std::vector<Code> code;

    // Use to reserve space for patching a PC.
    auto reserve = [&](size_t count = 1) {
      auto offset = code.size();
      code.insert(code.end(), count * 2, 0);
      return offset;
    };

    // Use to patch a PC into the code stream later.
    auto patch = [&](size_t& offset, uint64_t write) {
      code.at(offset++) = write & 0xFFFFFFFF;
      code.at(offset++) = (write >> 32) & 0xFFFFFFFF;
    };

    std::function<uint32_t(Node)> typ = [&](Node type) -> uint32_t {
      if (type == Dyn)
        return type::dyn();

      if (type == ClassId)
        return type::cls(*get_class_id(type));

      if (type == TypeId)
        return type::def(classes.size(), *get_type_id(type));

      if (type == Array)
        return type::array(typ(type / Type));

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

    // FFI string table.
    hdr << uleb(ST::ffi().size());

    for (size_t i = 0; i < ST::ffi().size(); i++)
      hdr << ST::ffi().at(i);

    // FFI libraries.
    hdr << uleb(libraries.size());

    for (auto& lib : libraries)
      hdr << uleb(ST::ffi().string(lib / String));

    hdr << uleb(symbols.size());

    for (auto& symbol : symbols)
    {
      hdr << uleb(*get_library_id(symbol->parent(Lib)))
          << uleb(ST::ffi().string(symbol / String))
          << uleb((symbol / FFIParams)->size());

      for (auto& param : *(symbol / FFIParams))
        hdr << uleb(typ(param));

      hdr << uleb(typ(symbol / Return));
    }

    // Function headers.
    hdr << uleb(functions.size());

    for (auto& func_state : functions)
    {
      hdr << uleb(func_state.params);
      hdr << uleb(func_state.register_idxs.size());
      hdr << uleb(func_state.label_idxs.size());

      // Parameter and return types.
      for (auto& param : *(func_state.func / Params))
        hdr << uleb(typ(param / Type));

      hdr << uleb(typ(func_state.func / Type));

      // Reserve space for a 64 bit PC for each label.
      func_state.label_pcs = reserve(func_state.label_idxs.size());

      // Reserve space for a 64 bit PC for the debug info.
      func_state.debug_offset = reserve();
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

    // Function bodies.
    // This goes in `code`, not in `hdr`.
    for (auto& func_state : functions)
    {
      patch(func_state.debug_offset, di.size());

      // Function name.
      di << uleb(func_state.name);

      // Register names.
      for (auto& name : func_state.register_names)
        di << uleb(name);

      auto dst = [&](Node stmt) {
        return *func_state.get_register_id(stmt / LocalId);
      };

      auto lhs = [&](Node stmt) {
        return *func_state.get_register_id(stmt / Lhs);
      };

      auto rhs = [&](Node stmt) {
        return *func_state.get_register_id(stmt / Rhs);
      };

      auto src = rhs;

      auto cls = [&](Node stmt) { return *get_class_id(stmt / ClassId); };

      auto fld = [&](Node stmt) { return *get_field_id(stmt / FieldId); };

      auto mth = [&](Node stmt) { return *get_method_id(stmt / MethodId); };

      auto fn = [&](Node stmt) { return *get_func_id(stmt / FunctionId); };

      auto args = [&](Node args) {
        for (auto arg : *args)
          code << e{Op::Arg, +argtype(arg), src(arg)};
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
        patch(func_state.label_pcs, code.size());

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
              code << e{Op::Const, dst(stmt), +val(t)};
            }
            else if (t == Bool)
            {
              if ((stmt / Rhs) == True)
                code << e{Op::Const, dst(stmt), +val(t), true};
              else
                code << e{Op::Const, dst(stmt), +val(t), false};
            }
            else if (t == I8)
            {
              code << e{
                Op::Const,
                dst(stmt),
                +val(t),
                static_cast<uint8_t>(lit<int8_t>(v))};
            }
            else if (t == U8)
            {
              code << e{Op::Const, dst(stmt), +val(t), lit<uint8_t>(v)};
            }
            else if (t == I16)
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<int16_t>(v);
            }
            else if (t == U16)
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<uint16_t>(v);
            }
            else if (t == I32)
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<int32_t>(v);
            }
            else if (t == U32)
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<uint32_t>(v);
            }
            else if (t->in({I64, ILong, ISize}))
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<int64_t>(v);
            }
            else if (t->in({U64, ULong, USize, Ptr}))
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<uint64_t>(v);
            }
            else if (t == F32)
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<float>(v);
            }
            else if (t == F64)
            {
              code << e{Op::Const, dst(stmt), +val(t)} << lit<double>(v);
            }
          }
          else if (stmt == Convert)
          {
            code << e{Op::Convert, dst(stmt), +val(stmt / Type), rhs(stmt)};
          }
          else if (stmt == Stack)
          {
            args(stmt / Args);
            code << e{Op::Stack, dst(stmt)} << cls(stmt);
          }
          else if (stmt == Heap)
          {
            args(stmt / Args);
            code << e{Op::Heap, dst(stmt), rhs(stmt)} << cls(stmt);
          }
          else if (stmt == Region)
          {
            args(stmt / Args);
            code << e{Op::Region, dst(stmt), rgn(stmt)} << cls(stmt);
          }
          else if (stmt == StackArray)
          {
            code << e{Op::StackArray, dst(stmt), rhs(stmt)} << typ(stmt / Type);
          }
          else if (stmt == StackArrayConst)
          {
            code << e{Op::StackArrayConst, dst(stmt)} << typ(stmt / Type)
                 << lit<uint64_t>(stmt / Rhs);
          }
          else if (stmt == HeapArray)
          {
            code << e{Op::HeapArray, dst(stmt), lhs(stmt), rhs(stmt)}
                 << typ(stmt / Type);
          }
          else if (stmt == HeapArrayConst)
          {
            code << e{Op::HeapArrayConst, dst(stmt), src(stmt)}
                 << typ(stmt / Type) << lit<uint64_t>(stmt / Rhs);
          }
          else if (stmt == RegionArray)
          {
            code << e{Op::RegionArray, dst(stmt), rgn(stmt), rhs(stmt)}
                 << typ(stmt / Type);
          }
          else if (stmt == RegionArrayConst)
          {
            code << e{Op::RegionArrayConst, dst(stmt), rgn(stmt)}
                 << typ(stmt / Type) << lit<uint64_t>(stmt / Rhs);
          }
          else if (stmt == Copy)
          {
            code << e{Op::Copy, dst(stmt), src(stmt)};
          }
          else if (stmt == Move)
          {
            code << e{Op::Move, dst(stmt), src(stmt)};
          }
          else if (stmt == Drop)
          {
            code << e{Op::Drop, dst(stmt)};
          }
          else if (stmt == Ref)
          {
            auto arg = stmt / Arg;
            code << e{Op::Ref, dst(stmt), +argtype(arg), src(arg)} << fld(stmt);
          }
          else if (stmt == ArrayRef)
          {
            auto arg = stmt / Arg;
            Op op;

            if (argtype(arg) == ArgType::Move)
              op = Op::ArrayRefMove;
            else
              op = Op::ArrayRefCopy;

            code << e{op, dst(stmt), src(arg), rhs(stmt)};
          }
          else if (stmt == ArrayRefConst)
          {
            auto arg = stmt / Arg;
            code << e{Op::ArrayRefConst, dst(stmt), +argtype(arg), src(arg)}
                 << lit<uint64_t>(stmt / Rhs);
          }
          else if (stmt == Load)
          {
            code << e{Op::Load, dst(stmt), rhs(stmt)};
          }
          else if (stmt == Store)
          {
            auto arg = stmt / Arg;

            if (argtype(arg) == ArgType::Move)
              code << e{Op::StoreMove, dst(stmt), src(stmt), src(arg)};
            else
              code << e{Op::StoreCopy, dst(stmt), src(stmt), src(arg)};
          }
          else if (stmt == FnPointer)
          {
            code << e{Op::Lookup, dst(stmt), +CallType::CallStatic} << fn(stmt);
          }
          else if (stmt == Lookup)
          {
            code << e{Op::Lookup, dst(stmt), +CallType::CallDynamic, src(stmt)}
                 << mth(stmt);
          }
          else if (stmt == Call)
          {
            args(stmt / Args);
            code << e{Op::Call, dst(stmt), +CallType::CallStatic} << fn(stmt);
          }
          else if (stmt == CallDyn)
          {
            args(stmt / Args);
            code << e{Op::Call, dst(stmt), +CallType::CallDynamic, src(stmt)};
          }
          else if (stmt == Subcall)
          {
            args(stmt / Args);
            code << e{Op::Call, dst(stmt), +CallType::SubcallStatic}
                 << fn(stmt);
          }
          else if (stmt == SubcallDyn)
          {
            args(stmt / Args);
            code << e{
              Op::Call, dst(stmt), +CallType::SubcallDynamic, src(stmt)};
          }
          else if (stmt == Try)
          {
            args(stmt / Args);
            code << e{Op::Call, dst(stmt), +CallType::TryStatic} << fn(stmt);
          }
          else if (stmt == TryDyn)
          {
            args(stmt / Args);
            code << e{Op::Call, dst(stmt), +CallType::TryDynamic, src(stmt)};
          }
          else if (stmt == FFI)
          {
            args(stmt / Args);
            code << e{Op::Call, dst(stmt), +CallType::FFI}
                 << *get_symbol_id(stmt / SymbolId);
          }
          else if (stmt == Add)
          {
            code << e{Op::Add, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Sub)
          {
            code << e{Op::Sub, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Mul)
          {
            code << e{Op::Mul, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Div)
          {
            code << e{Op::Div, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Mod)
          {
            code << e{Op::Mod, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Pow)
          {
            code << e{Op::Pow, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == And)
          {
            code << e{Op::And, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Or)
          {
            code << e{Op::Or, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Xor)
          {
            code << e{Op::Xor, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Shl)
          {
            code << e{Op::Shl, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Shr)
          {
            code << e{Op::Shr, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Eq)
          {
            code << e{Op::Eq, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Ne)
          {
            code << e{Op::Ne, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Lt)
          {
            code << e{Op::Lt, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Le)
          {
            code << e{Op::Le, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Gt)
          {
            code << e{Op::Gt, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Ge)
          {
            code << e{Op::Ge, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Min)
          {
            code << e{Op::Min, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Max)
          {
            code << e{Op::Max, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == LogBase)
          {
            code << e{Op::LogBase, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Atan2)
          {
            code << e{Op::Atan2, dst(stmt), lhs(stmt), rhs(stmt)};
          }
          else if (stmt == Neg)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Neg, lhs(stmt)};
          }
          else if (stmt == Not)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Not, lhs(stmt)};
          }
          else if (stmt == Abs)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Abs, lhs(stmt)};
          }
          else if (stmt == Ceil)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Ceil, lhs(stmt)};
          }
          else if (stmt == Floor)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Floor, lhs(stmt)};
          }
          else if (stmt == Exp)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Exp, lhs(stmt)};
          }
          else if (stmt == Log)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Log, lhs(stmt)};
          }
          else if (stmt == Sqrt)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Sqrt, lhs(stmt)};
          }
          else if (stmt == Cbrt)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Cbrt, lhs(stmt)};
          }
          else if (stmt == IsInf)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::IsInf, lhs(stmt)};
          }
          else if (stmt == IsNaN)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::IsNaN, lhs(stmt)};
          }
          else if (stmt == Sin)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Sin, lhs(stmt)};
          }
          else if (stmt == Cos)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Cos, lhs(stmt)};
          }
          else if (stmt == Tan)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Tan, lhs(stmt)};
          }
          else if (stmt == Asin)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Asin, lhs(stmt)};
          }
          else if (stmt == Acos)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Acos, lhs(stmt)};
          }
          else if (stmt == Atan)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Atan, lhs(stmt)};
          }
          else if (stmt == Sinh)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Sinh, lhs(stmt)};
          }
          else if (stmt == Cosh)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Cosh, lhs(stmt)};
          }
          else if (stmt == Tanh)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Tanh, lhs(stmt)};
          }
          else if (stmt == Asinh)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Asinh, lhs(stmt)};
          }
          else if (stmt == Acosh)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Acosh, lhs(stmt)};
          }
          else if (stmt == Atanh)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Atanh, lhs(stmt)};
          }
          else if (stmt == Const_E)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Const_E};
          }
          else if (stmt == Const_Pi)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Const_Pi};
          }
          else if (stmt == Const_Inf)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Const_Inf};
          }
          else if (stmt == Const_NaN)
          {
            code << e{Op::MathOp, dst(stmt), +MathOp::Const_NaN};
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
          code << e{Op::Tailcall, +CallType::CallStatic} << fn(term);
        }
        else if (term == TailcallDyn)
        {
          args(term / MoveArgs);
          code << e{Op::Tailcall, +CallType::CallDynamic, dst(term)};
        }
        else if (term == Return)
        {
          code << e{Op::Return, dst(term), +Condition::Return};
        }
        else if (term == Raise)
        {
          code << e{Op::Return, dst(term), +Condition::Raise};
        }
        else if (term == Throw)
        {
          code << e{Op::Return, dst(term), +Condition::Throw};
        }
        else if (term == Cond)
        {
          auto t = *func_state.get_label_id(term / Lhs);
          auto f = *func_state.get_label_id(term / Rhs);
          code << e{Op::Cond, dst(term), t, f};
        }
        else if (term == Jump)
        {
          code << e{Op::Jump, *func_state.get_label_id(term / LabelId)};
        }
      }
    }

    if constexpr (std::endian::native == std::endian::big)
    {
      for (size_t i = 0; i < code.size(); i++)
        code[i] = std::byteswap(code[i]);
    }

    // Length of the debug info section.
    if (options().strip)
      hdr << uleb(0);
    else
      hdr << uleb(di.size());

    std::ofstream f(options().bytecode_file, std::ios::binary | std::ios::out);

    f.write(reinterpret_cast<const char*>(hdr.data()), hdr.size());
    auto len = hdr.size();

    if (!options().strip)
    {
      f.write(reinterpret_cast<const char*>(di.data()), di.size());
      len += di.size();
    }

    // Pad to sizeof(Code) alignment.
    auto pad = (sizeof(Code) - (len % sizeof(Code))) % sizeof(Code);
    f.write("\0\0\0\0", pad);

    f.write(
      reinterpret_cast<const char*>(code.data()), code.size() * sizeof(Code));

    if (!f)
    {
      logging::Error() << "Error writing to: " << options().bytecode_file
                       << std::endl;
    }
  }
}
