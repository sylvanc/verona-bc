#include "lang.h"
#include "vbci.h"
#include "wf.h"

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

  inline std::vector<Code>& operator<<(std::vector<Code>& code, e&& e)
  {
    code.push_back(e);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, int16_t v)
  {
    code.push_back(v);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, uint16_t v)
  {
    code.push_back(v);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, int32_t v)
  {
    code.push_back(v);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, uint32_t v)
  {
    code.push_back(v);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, int64_t v)
  {
    code.push_back(v & 0xFFFFFFFF);
    code.push_back((v >> 32) & 0xFFFFFFFF);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, uint64_t v)
  {
    code.push_back(v & 0xFFFFFFFF);
    code.push_back((v >> 32) & 0xFFFFFFFF);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, float v)
  {
    auto t = std::bit_cast<uint32_t>(v);
    code.push_back(t);
    return code;
  }

  inline std::vector<Code>& operator<<(std::vector<Code>& code, double v)
  {
    auto t = std::bit_cast<uint64_t>(v);
    code.push_back(t);
    return code;
  }

  inline std::optional<uint8_t> val(Node ptype)
  {
    if (ptype == Type)
      ptype = ptype / Type;

    if (ptype == None)
      return +ValueType::None;
    if (ptype == Bool)
      return +ValueType::Bool;
    if (ptype == I8)
      return +ValueType::I8;
    if (ptype == I16)
      return +ValueType::I16;
    if (ptype == I32)
      return +ValueType::I32;
    if (ptype == I64)
      return +ValueType::I64;
    if (ptype == U8)
      return +ValueType::U8;
    if (ptype == U16)
      return +ValueType::U16;
    if (ptype == U32)
      return +ValueType::U32;
    if (ptype == U64)
      return +ValueType::U64;
    if (ptype == F32)
      return +ValueType::F32;
    if (ptype == F64)
      return +ValueType::F64;

    return {};
  }

  inline uint8_t rgn(Node node)
  {
    if (node == RegionRC)
      return +RegionType::RegionRC;
    if (node == RegionGC)
      return +RegionType::RegionGC;
    if (node == RegionArena)
      return +RegionType::RegionArena;

    return uint8_t(-1);
  }

  template<typename T>
  inline T lit(Node node)
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
  inline float lit<float>(Node node)
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
  inline double lit<double>(Node node)
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

  PassDef bytecode()
  {
    struct FuncState
    {
      Node func;
      uint8_t params;
      uint8_t labels;
      size_t pcs;
      std::unordered_map<std::string, uint8_t> label_idxs;
      std::unordered_map<std::string, uint8_t> register_idxs;

      FuncState(Node func) : func(func) {}

      std::optional<uint8_t> get_label_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = label_idxs.find(name);

        if (find == label_idxs.end())
          return {};

        return find->second;
      }

      bool add_label(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = label_idxs.find(name);

        if (find != label_idxs.end())
          return false;

        label_idxs.insert({name, label_idxs.size()});
        return true;
      }

      std::optional<uint8_t> get_register_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = register_idxs.find(name);

        if (find == register_idxs.end())
          return {};

        return find->second;
      }

      bool add_register(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = register_idxs.find(name);

        if (find != register_idxs.end())
          return false;

        register_idxs.insert({name, register_idxs.size()});
        return true;
      }
    };

    struct State
    {
      bool error = false;

      std::unordered_map<std::string, Id> func_ids;
      std::unordered_map<std::string, Id> class_ids;
      std::unordered_map<std::string, Id> field_ids;
      std::unordered_map<std::string, Id> method_ids;

      std::vector<Node> primitives;
      std::vector<Node> classes;
      std::vector<FuncState> functions;

      State()
      {
        // Reserve func_id 0 for `main`.
        functions.push_back(FuncState(nullptr));
        func_ids.insert({"@main", 0});
      }

      std::optional<Id> get_class_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = class_ids.find(name);

        if (find == class_ids.end())
          return {};

        return find->second;
      }

      bool add_class(Node cls)
      {
        auto name = std::string((cls / ClassId)->location().view());
        auto find = class_ids.find(name);

        if (find != class_ids.end())
          return false;

        class_ids.insert({name, class_ids.size()});
        classes.push_back(cls);
        return true;
      }

      std::optional<Id> get_field_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = field_ids.find(name);

        if (find == field_ids.end())
          return {};

        return find->second;
      }

      void add_field(Node field)
      {
        auto name = std::string((field / FieldId)->location().view());
        auto find = field_ids.find(name);

        if (find == field_ids.end())
          field_ids.insert({name, field_ids.size()});
      }

      std::optional<Id> get_method_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = method_ids.find(name);

        if (find == method_ids.end())
          return {};

        return find->second;
      }

      void add_method(Node method)
      {
        auto name = std::string((method / MethodId)->location().view());
        auto find = method_ids.find(name);

        if (find == method_ids.end())
          method_ids.insert({name, method_ids.size()});
      }

      std::optional<Id> get_func_id(Node id)
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

      FuncState& get_func(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = func_ids.find(name);
        return functions.at(find->second);
      }

      FuncState& add_func(Node func)
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
        func_state.labels = (func / Labels)->size();
        return func_state;
      }
    };

    auto state = std::make_shared<State>();
    state->primitives.resize(NumPrimitiveClasses);

    PassDef p{
      "bytecode",
      wfPassLabels,
      dir::topdown | dir::once,
      {
        // Accumulate primitive classes.
        T(Primitive)[Primitive] >> [state](Match& _) -> Node {
          auto primitive = _(Primitive);
          auto vtype = val(primitive / Type);

          if (!vtype)
          {
            state->error = true;
            return err(primitive / Type, "unknown primitive type");
          }

          if (state->primitives.at(*vtype))
          {
            state->error = true;
            return err(primitive / Type, "duplicate primitive class");
          }

          state->primitives[*vtype] = primitive;
          return NoChange;
        },

        // Accumulate user-defined classes.
        T(Class)[Class] >> [state](Match& _) -> Node {
          if (!state->add_class(_(Class)))
          {
            state->error = true;
            return err(_(Class) / ClassId, "duplicate class name");
          }
          return NoChange;
        },

        // Accumulate field names.
        T(Field)[Field] >> [state](Match& _) -> Node {
          state->add_field(_(Field));
          return NoChange;
        },

        // Accumulate method names.
        T(Method)[Method] >> [state](Match& _) -> Node {
          auto method = _(Method);
          state->add_method(method);

          if (!state->get_func_id(method / FunctionId))
          {
            state->error = true;
            return err(method / Rhs, "unknown function");
          }

          return NoChange;
        },

        // Accumulate functions.
        T(Func)[Func] >> [state](Match& _) -> Node {
          auto func = _(Func);
          auto func_id = func / FunctionId;

          if (state->get_func_id(func_id))
          {
            state->error = true;
            return err(func / FunctionId, "duplicate function name");
          }

          auto& func_state = state->add_func(func);

          if ((func / Labels)->size() == 0)
          {
            state->error = true;
            return err(func_id, "function has no labels");
          }

          if ((func / Labels)->size() >= MaxRegisters)
          {
            state->error = true;
            return err(func_id, "function has too many labels");
          }

          if ((func / Params)->size() >= MaxRegisters)
          {
            state->error = true;
            return err(func_id, "function has too many params");
          }

          // Register label names.
          for (auto label : *(func / Labels))
          {
            if (!func_state.add_label(label / LabelId))
            {
              state->error = true;
              return err(label / LabelId, "duplicate label name");
            }
          }

          // Register parameter names.
          for (auto param : *(func / Params))
          {
            if (!func_state.add_register(param / LocalId))
            {
              state->error = true;
              return err(param / LocalId, "duplicate parameter name");
            }
          }

          return NoChange;
        },

        // Check that all labels in a function are defined.
        T(LabelId)[LabelId] >> [state](Match& _) -> Node {
          auto label = _(LabelId);
          auto& func_state = state->get_func(label->parent(Func) / FunctionId);

          if (!func_state.get_label_id(label))
          {
            state->error = true;
            return err(label, "undefined label");
          }

          return NoChange;
        },

        // Define all destination registers.
        Def[Body] >> [state](Match& _) -> Node {
          auto stmt = _(Body);
          auto dst = stmt / LocalId;
          auto& func_state = state->get_func(dst->parent(Func) / FunctionId);

          if (func_state.add_register(dst))
          {
            // Check that no register used in this statement was just defined.
            for (auto& child : *stmt)
            {
              if ((&*dst != &*child) && dst->equals(child))
              {
                state->error = true;
                return err(child, "register used before defined");
              }
            }
          }

          return NoChange;
        },

        // Check that all registers in a function are defined.
        T(LocalId)[LocalId] >> [state](Match& _) -> Node {
          auto id = _(LocalId);
          auto& func_state = state->get_func(id->parent(Func) / FunctionId);

          if (!func_state.get_register_id(id))
          {
            state->error = true;
            return err(id, "undefined register");
          }

          return NoChange;
        },
      }};

    p.post([state](auto top) {
      if (state->error)
        return 0;

      if (!state->functions.at(0).func)
      {
        state->error = true;
        top << err(Func, "missing main function");
        return 0;
      }

      std::vector<Code> code;
      code << MagicNumber;
      code << CurrentVersion;

      // Function headers.
      code << uint32_t(state->functions.size());

      for (auto& func_state : state->functions)
      {
        // 8 bit label count, 8 bit param count.
        code << uint32_t((func_state.params << 8) | func_state.labels);

        // Reserve space for a 64 bit PC for each label.
        func_state.pcs = code.size();
        code.insert(code.end(), func_state.labels * 2, 0);
      }

      // Primitive classes.
      for (auto& p : state->primitives)
      {
        if (p)
        {
          auto methods = p / Methods;
          code << uint32_t(methods->size());

          for (auto& method : *methods)
          {
            code << *state->get_method_id(method / MethodId);
            code << *state->get_func_id(method / FunctionId);
          }
        }
        else
        {
          code << uint32_t(0);
        }
      }

      // Classes.
      code << uint32_t(state->classes.size());

      for (auto& c : state->classes)
      {
        auto fields = c / Fields;
        code << uint32_t(fields->size());

        for (auto& field : *fields)
          code << *state->get_field_id(field / FieldId);

        auto methods = c / Methods;
        code << uint32_t(methods->size());

        for (auto& method : *methods)
        {
          code << *state->get_method_id(method / MethodId);
          code << *state->get_func_id(method / FunctionId);
        }
      }

      // Function bodies.
      for (auto& func_state : state->functions)
      {
        auto dst = [&func_state](Node stmt) {
          return *func_state.get_register_id(stmt / LocalId);
        };

        auto lhs = [&func_state](Node stmt) {
          return *func_state.get_register_id(stmt / Lhs);
        };

        auto rhs = [&func_state](Node stmt) {
          return *func_state.get_register_id(stmt / Rhs);
        };

        auto src = rhs;

        auto cls = [state](Node stmt) {
          return *state->get_class_id(stmt / ClassId);
        };

        auto fld = [state](Node stmt) {
          return *state->get_field_id(stmt / FieldId);
        };

        auto mth = [state](Node stmt) {
          return *state->get_method_id(stmt / MethodId);
        };

        auto fn = [state](Node stmt) {
          return *state->get_func_id(stmt / FunctionId);
        };

        auto args = [state, &code, &src](Node stmt) {
          // Set up the arguments.
          auto args = stmt / Args;
          uint8_t i = 0;

          for (auto arg : *args)
          {
            ArgType t;

            if ((arg / Type) == Move)
              t = ArgType::Move;
            else
              t = ArgType::Copy;

            code << e{Op::Arg, i++, +t, src(arg)};
          }
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
              auto t = stmt / Type;
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
              code << e{Op::Stack, dst(stmt)} << cls(stmt);
            }
            else if (stmt == Heap)
            {
              code << e{Op::Heap, dst(stmt), rhs(stmt)} << cls(stmt);
            }
            else if (stmt == Region)
            {
              code << e{Op::Region, dst(stmt), rgn(stmt / Type)} << cls(stmt);
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
              code << e{Op::Ref, dst(stmt), src(stmt)} << fld(stmt);
            }
            else if (stmt == Load)
            {
              code << e{Op::Load, dst(stmt), rhs(stmt)};
            }
            else if (stmt == Store)
            {
              code << e{Op::Store, dst(stmt), lhs(stmt), rhs(stmt)};
            }
            else if (stmt == FnPointer)
            {
              code << e{Op::Lookup, dst(stmt), +CallType::CallStatic}
                   << fn(stmt);
            }
            else if (stmt == Lookup)
            {
              code << e{Op::Lookup,
                        dst(stmt),
                        +CallType::CallDynamic,
                        src(stmt)}
                   << mth(stmt);
            }
            else if (stmt == Call)
            {
              args(stmt);
              code << e{Op::Call, dst(stmt), +CallType::CallStatic} << fn(stmt);
            }
            else if (stmt == CallDyn)
            {
              args(stmt);
              code << e{Op::Call, dst(stmt), +CallType::CallDynamic, src(stmt)};
            }
            else if (stmt == Subcall)
            {
              args(stmt);
              code << e{Op::Call, dst(stmt), +CallType::SubcallStatic}
                   << fn(stmt);
            }
            else if (stmt == SubcallDyn)
            {
              args(stmt);
              code << e{
                Op::Call, dst(stmt), +CallType::SubcallDynamic, src(stmt)};
            }
            else if (stmt == Try)
            {
              args(stmt);
              code << e{Op::Call, dst(stmt), +CallType::TryStatic} << fn(stmt);
            }
            else if (stmt == TryDyn)
            {
              args(stmt);
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
            args(term);
            code << e{Op::Tailcall, +CallType::CallStatic} << fn(term);
          }
          else if (term == TailcallDyn)
          {
            args(term);
            code << e{Op::Tailcall, +CallType::CallDynamic, src(term)};
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

      std::ofstream f(
        options().bytecode_file, std::ios::binary | std::ios::out);

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

      return 0;
    });

    return p;
  }
}
