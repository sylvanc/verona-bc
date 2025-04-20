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
    auto name = std::string(id->location().view());
    auto find = label_idxs.find(name);

    if (find == label_idxs.end())
      return {};

    return find->second;
  }

  LabelState& FuncState::get_label(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = label_idxs.find(name);
    return labels.at(find->second);
  }

  bool FuncState::add_label(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = label_idxs.find(name);

    if (find != label_idxs.end())
      return false;

    label_idxs.insert({name, label_idxs.size()});
    labels.emplace_back();
    return true;
  }

  std::optional<uint8_t> FuncState::get_register_id(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = register_idxs.find(name);

    if (find == register_idxs.end())
      return {};

    return find->second;
  }

  bool FuncState::add_register(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = register_idxs.find(name);

    if (find != register_idxs.end())
      return false;

    register_idxs.insert({name, register_idxs.size()});
    return true;
  }

  State::State()
  {
    primitives.resize(NumPrimitiveClasses);

    // Reserve a function ID for `@main`.
    functions.push_back(FuncState(nullptr));
    func_ids.insert({"@main", MainFuncId});

    // Reserve a method ID for `@final`.
    method_ids.insert({"@final", FinalMethodId});
  }

  std::optional<Id> State::get_class_id(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = class_ids.find(name);

    if (find == class_ids.end())
      return {};

    return find->second;
  }

  bool State::add_class(Node cls)
  {
    auto name = std::string((cls / ClassId)->location().view());
    auto find = class_ids.find(name);

    if (find != class_ids.end())
      return false;

    class_ids.insert({name, class_ids.size()});
    classes.push_back(cls);
    return true;
  }

  std::optional<Id> State::get_field_id(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      return {};

    return find->second;
  }

  void State::add_field(Node field)
  {
    auto name = std::string((field / FieldId)->location().view());
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      field_ids.insert({name, field_ids.size()});
  }

  std::optional<Id> State::get_method_id(Node id)
  {
    auto name = std::string(id->location().view());
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      return {};

    return find->second;
  }

  void State::add_method(Node method)
  {
    auto name = std::string((method / MethodId)->location().view());
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      method_ids.insert({name, method_ids.size()});
  }

  std::optional<Id> State::get_func_id(Node id)
  {
    auto name = std::string(id->location().view());
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
    auto name = std::string(id->location().view());
    auto find = func_ids.find(name);
    return functions.at(find->second);
  }

  FuncState& State::add_func(Node func)
  {
    auto name = std::string((func / FunctionId)->location().view());
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
    func_state.params = (func / Params)->size();
    return func_state;
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
    std::vector<Code> code;
    code << MagicNumber;
    code << CurrentVersion;

    // Function headers.
    code << uint32_t(functions.size());

    for (auto& func_state : functions)
    {
      // 8 bit label count, 8 bit param count, 8 bit register count.
      auto labels = func_state.label_idxs.size();
      code << uint32_t(
        labels | (func_state.params << 8) |
        (func_state.register_idxs.size() << 16));

      // Reserve space for a 64 bit PC for each label.
      func_state.pcs = code.size();
      code.insert(code.end(), labels * 2, 0);
    }

    // Primitive classes.
    for (auto& p : primitives)
    {
      if (p)
      {
        auto methods = p / Methods;
        code << uint32_t(methods->size());

        for (auto& method : *methods)
        {
          code << *get_method_id(method / MethodId);
          code << *get_func_id(method / FunctionId);
        }
      }
      else
      {
        code << uint32_t(0);
      }
    }

    // Classes.
    code << uint32_t(classes.size());

    for (auto& c : classes)
    {
      auto fields = c / Fields;
      code << uint32_t(fields->size());

      for (auto& field : *fields)
        code << *get_field_id(field / FieldId);

      auto methods = c / Methods;
      code << uint32_t(methods->size());

      for (auto& method : *methods)
      {
        code << *get_method_id(method / MethodId);
        code << *get_func_id(method / FunctionId);
      }
    }

    // Function bodies.
    for (auto& func_state : functions)
    {
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
        // Set up the arguments.
        uint8_t i = 0;

        for (auto arg : *args)
          code << e{Op::Arg, i++, +argtype(arg), src(arg)};
      };

      for (auto label : *(func_state.func / Labels))
      {
        // Save the pc for this label.
        auto pc = code.size();
        code.at(func_state.pcs++) = pc & 0xFFFFFFFF;
        code.at(func_state.pcs++) = (pc >> 32) & 0xFFFFFFFF;

        for (auto stmt : *(label / Body))
        {
          if (stmt == Const)
          {
            auto t = stmt / Type / Type;
            auto v = stmt / Rhs;

            if (t == None)
            {
              code << e{Op::Const, dst(stmt), *val(t)};
            }
            else if (t == Bool)
            {
              if ((stmt / Rhs) == True)
                code << e{Op::Const, dst(stmt), *val(t), true};
              else
                code << e{Op::Const, dst(stmt), *val(t), false};
            }
            else if (t == I8)
            {
              code << e{
                Op::Const,
                dst(stmt),
                *val(t),
                static_cast<uint8_t>(lit<int8_t>(v))};
            }
            else if (t == U8)
            {
              code << e{Op::Const, dst(stmt), *val(t), lit<uint8_t>(v)};
            }
            else if (t == I16)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<int16_t>(v);
            }
            else if (t == U16)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<uint16_t>(v);
            }
            else if (t == I32)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<int32_t>(v);
            }
            else if (t == U32)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<uint32_t>(v);
            }
            else if (t == I64)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<int64_t>(v);
            }
            else if (t == U64)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<uint64_t>(v);
            }
            else if (t == F32)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<float>(v);
            }
            else if (t == F64)
            {
              code << e{Op::Const, dst(stmt), *val(t)} << lit<double>(v);
            }
          }
          else if (stmt == Convert)
          {
            code << e{Op::Convert, dst(stmt), *val(stmt / Type), rhs(stmt)};
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
            code << e{Op::StackArray, dst(stmt), rhs(stmt)};
          }
          else if (stmt == StackArrayConst)
          {
            code << e{Op::StackArrayConst, dst(stmt)}
                 << lit<uint64_t>(stmt / Rhs);
          }
          else if (stmt == HeapArray)
          {
            code << e{Op::HeapArray, dst(stmt), lhs(stmt), rhs(stmt)}
                 << rhs(stmt);
          }
          else if (stmt == HeapArrayConst)
          {
            code << e{Op::HeapArrayConst, dst(stmt), src(stmt)}
                 << lit<uint64_t>(stmt / Rhs);
          }
          else if (stmt == RegionArray)
          {
            code << e{Op::RegionArray, dst(stmt), rgn(stmt), rhs(stmt)};
          }
          else if (stmt == RegionArrayConst)
          {
            code << e{Op::RegionArrayConst, dst(stmt), rgn(stmt)}
                 << lit<uint64_t>(stmt / Rhs);
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

    std::ofstream f(options().bytecode_file, std::ios::binary | std::ios::out);

    if (f)
    {
      auto data = reinterpret_cast<const char*>(code.data());
      auto size = code.size() * sizeof(Code);
      f.write(data, size);
    }

    if (!f)
    {
      logging::Error() << "Error writing to: " << options().bytecode_file
                       << std::endl;
    }
  }
}
