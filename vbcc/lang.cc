#include "lang.h"

#include "vbci.h"
#include "wf.h"

#include <CLI/CLI.hpp>

namespace vbcc
{
  const auto IntType = T(I8, I16, I32, I64, U8, U16, U32, U64);
  const auto FloatType = T(F32, F64);
  const auto PrimitiveType = T(None, Bool) / IntType / FloatType;

  const auto IntLiteral = T(Bin, Oct, Hex, Int);
  const auto FloatLiteral = T(Float, HexFloat);

  const auto Binop =
    T(Add,
      Sub,
      Mul,
      Div,
      Mod,
      Pow,
      And,
      Or,
      Xor,
      Shl,
      Shr,
      Eq,
      Ne,
      Lt,
      Le,
      Gt,
      Ge,
      Min,
      Max,
      LogBase,
      Atan2);

  const auto Unop =
    T(Neg,
      Not,
      Abs,
      Ceil,
      Floor,
      Exp,
      Log,
      Sqrt,
      Cbrt,
      IsInf,
      IsNaN,
      Sin,
      Cos,
      Tan,
      Asin,
      Acos,
      Atan,
      Sinh,
      Cosh,
      Tanh,
      Asinh,
      Acosh,
      Atanh);

  const auto Constant = T(Const_E, Const_Pi, Const_Inf, Const_NaN);

  const auto Statement = Unop / Binop / Constant /
    T(Global,
      Const,
      Convert,
      Stack,
      Heap,
      Region,
      Copy,
      Move,
      Drop,
      Ref,
      Load,
      Store,
      Lookup,
      Arg,
      Call);

  const auto Terminator = T(Tailcall, Return, Cond, Jump);
  const auto Dst = T(LocalId)[LocalId] * T(Equals);

  Node err(Node node, const std::string& msg)
  {
    return Error << (ErrorMsg ^ msg) << node;
  }

  Options& options()
  {
    static Options opts;
    return opts;
  }

  void Options::configure(CLI::App& cli)
  {
    cli.add_flag(
      "-b,--bytecode", bytecode_file, "Output bytecode to this file.");
  }

  PassDef statements()
  {
    return {
      "statements",
      wfPassStatements,
      dir::bottomup,
      {
        T(Directory, File, Group)[Group] >>
          [](Match& _) { return Seq << *_[Group]; },

        // Primitive class.
        (T(Primitive) << End) * PrimitiveType[Type] >>
          [](Match& _) { return Primitive << (Type << _(Type)) << Methods; },

        (T(Primitive) << T(Type))[Primitive] * T(GlobalId)[Lhs] *
            T(GlobalId)[Rhs] >>
          [](Match& _) {
            (_(Primitive) / Methods) << (Method << _(Lhs) << _(Rhs));
            return _(Primitive);
          },

        // User-defined class.
        (T(Class) << End) * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Class << _(GlobalId) << Fields << Methods; },

        (T(Class) << T(GlobalId))[Class] * T(GlobalId)[GlobalId] * T(Colon) *
            PrimitiveType[Type] >>
          [](Match& _) {
            (_(Class) / Fields) << (Field << _(GlobalId) << (Type << _(Type)));
            return _(Class);
          },

        (T(Class) << T(GlobalId))[Class] * T(GlobalId)[Lhs] *
            T(GlobalId)[Rhs] >>
          [](Match& _) {
            (_(Class) / Methods) << (Method << _(Lhs) << _(Rhs));
            return _(Class);
          },

        // Function.
        T(Func) * T(GlobalId)[GlobalId] * T(LParen) * T(Param)++[Param] *
            T(RParen) * T(Colon) * PrimitiveType[Type] >>
          [](Match& _) {
            return Func << _(GlobalId) << (Params << _[Param])
                        << (Type << _(Type)) << Labels;
          },

        // Parameter.
        T(LocalId)[LocalId] * T(Colon) * PrimitiveType[Type] >>
          [](Match& _) { return Param << _(LocalId) << (Type << _(Type)); },

        // Argument.
        (T(Move, Copy) * T(LocalId))[Arg] >>
          [](Match& _) { return Arg << _[Arg]; },

        // Strip commas between parameters and arguments.
        T(Param, Arg)[Lhs] * T(Comma) * T(Param, Arg)[Rhs] >>
          [](Match& _) { return Seq << _(Lhs) << _(Rhs); },

        // Globals.
        Dst * T(Global) * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Global << _(LocalId) << _(GlobalId); },

        // Constants.
        Dst * T(Const) * T(None) >>
          [](Match& _) {
            return Const << _(LocalId) << (Type << None) << None;
          },

        Dst * T(Const) * T(Bool) * T(True, False)[Rhs] >>
          [](Match& _) {
            return Const << _(LocalId) << (Type << Bool) << _(Rhs);
          },

        Dst * T(Const) * IntType[Type] * IntLiteral[Rhs] >>
          [](Match& _) {
            return Const << _(LocalId) << (Type << _(Type)) << _(Rhs);
          },

        Dst * T(Const) * FloatType[Type] * FloatLiteral[Rhs] >>
          [](Match& _) {
            return Const << _(LocalId) << (Type << _(Type)) << _(Rhs);
          },

        // Convert.
        Dst * T(Convert) * PrimitiveType[Type] * T(LocalId)[Rhs] >>
          [](Match& _) { return Convert << _(LocalId) << _(Type) << _(Rhs); },

        // Allocation.
        Dst * T(Stack) * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Stack << _(LocalId) << _(GlobalId); },

        Dst * T(Heap) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Heap << _(LocalId) << _(Rhs) << _(GlobalId); },

        Dst * T(Region) * T(RegionRC, RegionGC, RegionArena)[Rhs] *
            T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Region << _(LocalId) << _(Rhs) << _(GlobalId);
          },

        // Register operations.
        Dst * T(Copy) * T(LocalId)[Rhs] >>
          [](Match& _) { return Copy << _(LocalId) << _(Rhs); },

        Dst * T(Move) * T(LocalId)[Rhs] >>
          [](Match& _) { return Move << _(LocalId) << _(Rhs); },

        T(Drop) * T(LocalId)[LocalId] >>
          [](Match& _) { return Drop << _(LocalId); },

        // Reference operations.
        Dst * T(Ref) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Ref << _(LocalId) << _(Rhs) << _(GlobalId); },

        Dst * T(Load) * T(LocalId)[Rhs] >>
          [](Match& _) { return Load << _(LocalId) << _(Rhs); },

        Dst * T(Store) * T(LocalId)[Lhs] * T(LocalId)[Rhs] >>
          [](Match& _) { return Store << _(LocalId) << _(Lhs) << _(Rhs); },

        // Static lookup.
        Dst * T(Lookup) * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Lookup << _(LocalId) << None << _(GlobalId); },

        // Dynamic lookup.
        Dst * T(Lookup) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Lookup << _(LocalId) << _(Rhs) << _(GlobalId);
          },

        // Argument.
        T(Arg) * T(Int)[Int] * T(Move, Copy)[Move] * T(LocalId)[Rhs] >>
          [](Match& _) { return Arg << _(Int) << _(Move) << _(Rhs); },

        // Call.
        // TODO: return/raise/throw
        Dst * T(Call) * T(GlobalId, LocalId)[Lhs] * T(LParen) * T(Arg)++[Args] *
            T(RParen) >>
          [](Match& _) {
            return Call << _(LocalId) << _(Lhs) << (Args << _[Args]);
          },

        // Terminators.
        T(Tailcall) * T(GlobalId, LocalId)[Lhs] * T(LParen) * T(Arg)++[Args] *
            T(RParen) >>
          [](Match& _) { return Tailcall << _(Lhs) << (Args << _[Args]); },

        T(Return) * T(LocalId)[LocalId] >>
          [](Match& _) { return Return << _(LocalId); },

        (T(Cond) << End) * T(LocalId)[LocalId] * T(LabelId)[Lhs] *
            T(LabelId)[Rhs] >>
          [](Match& _) { return Cond << _(LocalId) << _(Lhs) << _(Rhs); },

        (T(Jump) << End) * T(LabelId)[LabelId] >>
          [](Match& _) { return Jump << _(LabelId); },

        // Binary operator.
        Dst * Binop[Type] * T(LocalId)[Lhs] * T(LocalId)[Rhs] >>
          [](Match& _) { return _(Type) << _(LocalId) << _(Lhs) << _(Rhs); },

        // Unary operator.
        Dst * Unop[Type] * T(LocalId)[Rhs] >>
          [](Match& _) { return _(Type) << _(LocalId) << _(Rhs); },

        // Constant.
        Dst * Constant[Type] >> [](Match& _) { return _(Type) << _(LocalId); },
      }};
  }

  PassDef labels()
  {
    return {
      "labels",
      wfPassLabels,
      dir::bottomup,
      {
        // Function.
        (T(Func)[Func]
         << (T(GlobalId) * T(Params) * T(Type) * T(Labels)[Labels])) *
            T(Label)[Label] >>
          [](Match& _) {
            _(Labels) << _(Label);
            return _(Func);
          },

        // Label.
        T(LabelId)[LabelId] * Statement++[Lhs] * Terminator[Rhs] >>
          [](Match& _) {
            return Label << _(LabelId) << (Body << _[Lhs]) << _(Rhs);
          },
      }};
  }

  PassDef bytecode()
  {
    using namespace vbci;

    struct State
    {
      bool error = false;
      FuncId func_id = 0;
      ClassId class_id = 0;
      FieldId field_id = 0;
      MethodId method_id = 0;

      std::unordered_map<std::string, FuncId> func_ids;
      std::unordered_map<std::string, ClassId> class_ids;
      std::unordered_map<std::string, FieldId> field_ids;
      std::unordered_map<std::string, MethodId> method_ids;

      std::vector<Node> functions;
      std::vector<Node> primitives;
      std::vector<Node> classes;

      std::optional<FuncId> get_func_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = func_ids.find(name);

        if (find == func_ids.end())
          return {};

        return find->second;
      }

      void add_func(Node func)
      {
        auto name = std::string((func / GlobalId)->location().view());
        func_ids.insert({name, func_id++});
        functions.push_back(func);
        assert(func_id == functions.size());
      }

      std::optional<ClassId> get_class_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = class_ids.find(name);

        if (find == class_ids.end())
          return {};

        return find->second;
      }

      void add_class(Node cls)
      {
        auto name = std::string((cls / GlobalId)->location().view());
        class_ids.insert({name, class_id++});
        classes.push_back(cls);
        assert(class_id == classes.size());
      }

      std::optional<FieldId> get_field_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = field_ids.find(name);

        if (find == field_ids.end())
          return {};

        return find->second;
      }

      void add_field(Node field)
      {
        auto name = std::string((field / GlobalId)->location().view());
        auto find = field_ids.find(name);

        if (find == field_ids.end())
          field_ids.insert({name, field_id++});
      }

      std::optional<MethodId> get_method_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = method_ids.find(name);

        if (find == method_ids.end())
          return {};

        return find->second;
      }

      void add_method(Node method)
      {
        auto name = std::string((method / Lhs)->location().view());
        auto find = method_ids.find(name);

        if (find == method_ids.end())
          method_ids.insert({name, method_id++});
      }
    };

    auto state = std::make_shared<State>();
    state->primitives.resize(NumPrimitiveClasses);

    PassDef p{
      "bytecode",
      wfPassLabels,
      dir::topdown | dir::once,
      {
        // Accumulate functions.
        T(Func)[Func] >> [state](Match& _) -> Node {
          auto func = _(Func);

          if (state->get_func_id(func / GlobalId))
          {
            state->error = true;
            return err(func / GlobalId, "duplicate function name");
          }

          state->add_func(func);
          return NoChange;
        },

        // Accumulate primitive classes.
        T(Primitive)[Primitive] >> [state](Match& _) -> Node {
          auto primitive = _(Primitive);
          auto ptype = primitive / Type / Type;
          ValueType vtype;

          if (ptype == None)
            vtype = ValueType::None;
          else if (ptype == Bool)
            vtype = ValueType::Bool;
          else if (ptype == I8)
            vtype = ValueType::I8;
          else if (ptype == I16)
            vtype = ValueType::I16;
          else if (ptype == I32)
            vtype = ValueType::I32;
          else if (ptype == I64)
            vtype = ValueType::I64;
          else if (ptype == U8)
            vtype = ValueType::U8;
          else if (ptype == U16)
            vtype = ValueType::U16;
          else if (ptype == U32)
            vtype = ValueType::U32;
          else if (ptype == U64)
            vtype = ValueType::U64;
          else if (ptype == F32)
            vtype = ValueType::F32;
          else if (ptype == F64)
            vtype = ValueType::F64;
          else
          {
            state->error = true;
            return err(ptype, "unknown primitive type");
          }

          auto idx = static_cast<size_t>(vtype);
          if (state->primitives.at(idx))
          {
            state->error = true;
            return err(ptype, "duplicate primitive class");
          }

          state->primitives[idx] = primitive;
          return NoChange;
        },

        // Accumulate user-defined classes.
        T(Class)[Class] >> [state](Match& _) -> Node {
          auto cls = _(Class);

          if (state->get_class_id(cls / GlobalId))
          {
            state->error = true;
            return err(cls / GlobalId, "duplicate class name");
          }

          state->add_class(cls);
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

          if (!state->get_func_id(method / Rhs))
          {
            state->error = true;
            return err(method / Rhs, "unknown function");
          }

          return NoChange;
        },
      }};

    p.post([state](auto) {
      if (state->error)
        return 0;

      std::vector<Code> code;
      code.push_back(MagicNumber);
      code.push_back(CurrentVersion);

      // Functions.
      code.push_back(state->functions.size());

      for (auto& f : state->functions)
      {
        // TODO: 8 bit label count, 8 bit param count.
        // TODO: 64 bit PC for each label.
        (void)f;
      }

      // Primitive classes.
      for (auto& p : state->primitives)
      {
        if (p)
        {
          auto methods = p / Methods;
          code.push_back(methods->size());

          for (auto& method : *methods)
          {
            code.push_back(state->get_method_id(method / Lhs).value());
            code.push_back(state->get_func_id(method / Rhs).value());
          }
        }
        else
        {
          code.push_back(0);
        }
      }

      // Classes.
      for (auto& c : state->classes)
      {
        auto fields = c / Fields;
        code.push_back(fields->size());

        for (auto& field : *fields)
          code.push_back(state->get_field_id(field / GlobalId).value());

        auto methods = c / Methods;
        code.push_back(methods->size());

        for (auto& method : *methods)
        {
          code.push_back(state->get_method_id(method / Lhs).value());
          code.push_back(state->get_func_id(method / Rhs).value());
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
