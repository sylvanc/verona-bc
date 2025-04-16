#include "lang.h"

#include "wf.h"

#include <CLI/CLI.hpp>

namespace vbcc
{
  const auto Dst = T(LocalId)[LocalId] * T(Equals);

  Node err(Node node, const std::string& msg)
  {
    return Error << (ErrorMsg ^ msg) << node;
  }

  template<typename T>
  Node check_int(Node value)
  {
    auto view = value->location().view();
    auto first = view.data();
    auto last = first + view.size();
    std::from_chars_result r;
    T t;

    if (value == Bin)
      r = std::from_chars(first + 2, last, t, 2);
    else if (value == Oct)
      r = std::from_chars(first + 2, last, t, 8);
    else if (value == Hex)
      r = std::from_chars(first + 2, last, t, 16);
    else if (value == Int)
      r = std::from_chars(first, last, t, 10);

    if (r.ec == std::errc())
      return {};

    if (r.ec == std::errc::result_out_of_range)
      return err(value, "Integer literal out of range.");

    return err(value, "Invalid integer literal.");
  }

  template<typename T>
  Node check_float(Node value)
  {
    auto view = value->location().view();
    auto first = view.data();
    auto last = first + view.size();
    std::from_chars_result r;
    T t;

    if (value == Float)
      r = std::from_chars(first, last, t);
    else if (value == HexFloat)
      r = std::from_chars(first + 2, last, t);

    if (r.ec == std::errc())
      return {};

    if (r.ec == std::errc::result_out_of_range)
      return err(value, "Floating point literal out of range.");

    return err(value, "Invalid floating point literal.");
  }

  Node check_literal(Node ty, Node value)
  {
    if (ty == I8)
      return check_int<int8_t>(value);
    if (ty == I16)
      return check_int<int16_t>(value);
    if (ty == I32)
      return check_int<int32_t>(value);
    if (ty == I64)
      return check_int<int64_t>(value);
    if (ty == U8)
      return check_int<uint8_t>(value);
    if (ty == U16)
      return check_int<uint16_t>(value);
    if (ty == U32)
      return check_int<uint32_t>(value);
    if (ty == U64)
      return check_int<uint64_t>(value);
    if (ty == F32)
      return check_float<float>(value);
    if (ty == F64)
      return check_float<double>(value);

    return {};
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
            (_(Primitive) / Methods)
              << (Method << (MethodId ^ _(Lhs)) << (FunctionId ^ _(Rhs)));
            return _(Primitive);
          },

        // User-defined class.
        (T(Class) << End) * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Class << (ClassId ^ _(GlobalId)) << Fields << Methods;
          },

        (T(Class) << T(ClassId))[Class] * T(GlobalId)[GlobalId] * T(Colon) *
            PrimitiveType[Type] >>
          [](Match& _) {
            (_(Class) / Fields)
              << (Field << (FieldId ^ _(GlobalId)) << (Type << _(Type)));
            return _(Class);
          },

        (T(Class) << T(ClassId))[Class] * T(GlobalId)[Lhs] * T(GlobalId)[Rhs] >>
          [](Match& _) {
            (_(Class) / Methods)
              << (Method << (MethodId ^ _(Lhs)) << (FunctionId ^ _(Rhs)));
            return _(Class);
          },

        // Function.
        T(Func) * T(GlobalId)[GlobalId] * T(LParen) * T(Param)++[Param] *
            T(RParen) * T(Colon) * PrimitiveType[Type] >>
          [](Match& _) {
            return Func << (FunctionId ^ _(GlobalId)) << (Params << _[Param])
                        << (Type << _(Type)) << Labels;
          },

        // Parameter.
        T(LocalId)[LocalId] * T(Colon) * PrimitiveType[Type] >>
          [](Match& _) { return Param << _(LocalId) << (Type << _(Type)); },

        // Argument.
        --T(Equals) * (T(Move) * T(LocalId)[LocalId]) >>
          [](Match& _) { return Arg << ArgMove << _(LocalId); },

        --T(Equals) * (T(Copy) * T(LocalId)[LocalId]) >>
          [](Match& _) { return Arg << ArgCopy << _(LocalId); },

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
            auto r = check_literal(_(Type), _(Rhs));
            if (r)
              return r;

            return Const << _(LocalId) << (Type << _(Type)) << _(Rhs);
          },

        Dst * T(Const) * FloatType[Type] * FloatLiteral[Rhs] >>
          [](Match& _) {
            auto r = check_literal(_(Type), _(Rhs));
            if (r)
              return r;

            return Const << _(LocalId) << (Type << _(Type)) << _(Rhs);
          },

        // Convert.
        Dst * T(Convert) * PrimitiveType[Type] * T(LocalId)[Rhs] >>
          [](Match& _) { return Convert << _(LocalId) << _(Type) << _(Rhs); },

        // Allocation.
        Dst * T(Stack) * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Stack << _(LocalId) << (ClassId ^ _(GlobalId));
          },

        Dst * T(Heap) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Heap << _(LocalId) << _(Rhs) << (ClassId ^ _(GlobalId));
          },

        Dst * T(Region) * T(RegionRC, RegionGC, RegionArena)[Rhs] *
            T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Region << _(LocalId) << _(Rhs) << (ClassId ^ _(GlobalId));
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
          [](Match& _) {
            return Ref << _(LocalId) << _(Rhs) << (FieldId ^ _(GlobalId));
          },

        Dst * T(Load) * T(LocalId)[Rhs] >>
          [](Match& _) { return Load << _(LocalId) << _(Rhs); },

        Dst * T(Store) * T(LocalId)[Lhs] * T(LocalId)[Rhs] >>
          [](Match& _) { return Store << _(LocalId) << _(Lhs) << _(Rhs); },

        // Static lookup.
        Dst * T(Lookup) * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return FnPointer << _(LocalId) << (FunctionId ^ _(GlobalId));
          },

        // Dynamic lookup.
        Dst * T(Lookup) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return Lookup << _(LocalId) << _(Rhs) << (MethodId ^ _(GlobalId));
          },

        // Static call.
        // TODO: return/raise/throw
        Dst * T(Call) * T(GlobalId)[GlobalId] * T(LParen) * T(Arg)++[Args] *
            T(RParen) >>
          [](Match& _) {
            return Call << _(LocalId) << (FunctionId ^ _(GlobalId))
                        << (Args << _[Args]);
          },

        // Dynamic call.
        Dst * T(Call) * T(LocalId)[Lhs] * T(LParen) * T(Arg)++[Args] *
            T(RParen) >>
          [](Match& _) {
            return CallDyn << _(LocalId) << _(Lhs) << (Args << _[Args]);
          },

        // Terminators.
        T(Tailcall) * T(GlobalId)[GlobalId] * T(LParen) * T(Arg)++[Args] *
            T(RParen) >>
          [](Match& _) {
            return Tailcall << (FunctionId ^ _(GlobalId)) << (Args << _[Args]);
          },

        T(Tailcall) * T(LocalId)[LocalId] * T(LParen) * T(Arg)++[Args] *
            T(RParen) >>
          [](Match& _) { return TailcallDyn << _(Lhs) << (Args << _[Args]); },

        T(Return) * T(LocalId)[LocalId] >>
          [](Match& _) { return Return << _(LocalId); },

        T(Raise) * T(LocalId)[LocalId] >>
          [](Match& _) { return Raise << _(LocalId); },

        T(Throw) * T(LocalId)[LocalId] >>
          [](Match& _) { return Throw << _(LocalId); },

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
        T(Func)[Func] * T(Label)[Label] >>
          [](Match& _) {
            (_(Func) / Labels) << _(Label);
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
