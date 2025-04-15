#include "lang.h"

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
}
