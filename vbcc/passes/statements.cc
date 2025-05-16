#include "../lang.h"

#ifndef __cpp_lib_from_chars
#include "../from_chars.h"
#endif

namespace vbcc
{
  const auto IntType =
    T(I8, I16, I32, I64, U8, U16, U32, U64, ILong, ULong, ISize, USize);
  const auto FloatType = T(F32, F64);
  const auto PrimitiveType = T(None, Bool) / IntType / FloatType;
  const auto TypeNoUnion =
    PrimitiveType / T(Ptr, Dyn, GlobalId) / (T(Array, Ref, Cown) << Any);
  const auto TypePat = (TypeNoUnion / (T(Union) << Any)) * --(T(Union) << End);

  const auto IntLiteral = T(Bin, Oct, Hex, Int);
  const auto FloatLiteral = T(Float, HexFloat);

  const auto Dst = T(LocalId)[LocalId] * T(Equals);
  const auto RegionType = T(RegionRC, RegionGC, RegionArena);
  const auto ArrayDynArg = T(LBracket) * T(LocalId)[Rhs] * T(RBracket);
  const auto ArrayConstArg = T(LBracket) * IntLiteral[Rhs] * T(RBracket);
  const auto SymbolParams = T(LParen) *
    (~(TypePat * (T(Comma) * TypePat)++))[Params] *
    ~(T(Comma) * T(Vararg)[Vararg]) * T(RParen);
  const auto ParamDef =
    T(LParen) * ~(T(Param) * (T(Comma) * T(Param))++) * T(RParen);
  const auto CallArgs =
    T(LParen) * ~(T(LocalId) * (T(Comma) * T(LocalId))++) * T(RParen);

  // clang-format off
  const auto wfPassStatements =
      wfIR
    | (Top <<= (
        Lib | Primitive | Class | Type | Func | LabelId | wfStatement
      | wfTerminator)++)
    ;
  // clang-format on

  template<typename T>
  Node check_int(Node value)
  {
    #ifdef __cpp_lib_from_chars
    using namespace std;
    #endif
    auto view = value->location().view();
    auto first = view.data();
    auto last = first + view.size();
    from_chars_result r;
    T t;

    if (value == Bin)
      r = from_chars(first + 2, last, t, 2);
    else if (value == Oct)
      r = from_chars(first + 2, last, t, 8);
    else if (value == Hex)
      r = from_chars(first + 2, last, t, 16);
    else if (value == Int)
      r = from_chars(first, last, t, 10);

    if (r.ec == std::errc())
      return {};

    if (r.ec == std::errc::result_out_of_range)
      return err(value, "Integer literal out of range.");

    return err(value, "Invalid integer literal.");
  }

  template<typename T>
  Node check_float(Node value)
  {
    #ifdef __cpp_lib_from_chars
    using namespace std;
    #endif
    auto view = value->location().view();
    auto first = view.data();
    auto last = first + view.size();
    from_chars_result r;
    T t;

    if (value == Float)
      r = from_chars(first, last, t);
    else if (value == HexFloat)
      r = from_chars(first + 2, last, t);

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
    if (ty->in({I64, ILong, ISize}))
      return check_int<int64_t>(value);
    if (ty == U8)
      return check_int<uint8_t>(value);
    if (ty == U16)
      return check_int<uint16_t>(value);
    if (ty == U32)
      return check_int<uint32_t>(value);
    if (ty->in({U64, ULong, USize}))
      return check_int<uint64_t>(value);
    if (ty == F32)
      return check_float<float>(value);
    if (ty == F64)
      return check_float<double>(value);

    return {};
  }

  Node symbolparams(NodeRange params)
  {
    Node r = FFIParams;

    for (auto& param : params)
    {
      if (param != Comma)
        r << param;
    }

    return r;
  }

  Node paramdef(NodeRange params)
  {
    Node r = Params;

    for (auto& param : params)
    {
      if (param == Param)
        r << param;
    }

    return r;
  }

  Node callargs(NodeRange args)
  {
    Node r = Args;

    for (auto& arg : args)
    {
      if (arg == LocalId)
        r << (Arg << ArgCopy << arg);
    }

    return r;
  }

  Node tailargs(NodeRange args)
  {
    Node r = MoveArgs;

    for (auto& arg : args)
    {
      if (arg == LocalId)
        r << (MoveArg << ArgMove << arg);
    }

    return r;
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

        // Array type.
        T(LBracket) * TypePat[Type] * T(RBracket) >>
          [](Match& _) { return Array << _(Type); },

        // Ref type.
        T(Ref) * TypePat[Type] >> [](Match& _) { return Ref << _(Type); },

        // Cown type.
        T(Cown) * TypePat[Type] >> [](Match& _) { return Cown << _(Type); },

        // Merge union types.
        (T(Union) << Any)[Lhs] * (T(Union) << End) * (T(Union) << Any)[Rhs] >>
          [](Match& _) { return _(Lhs) << *_[Rhs]; },

        // Extend a left-side union type.
        (T(Union) << Any)[Lhs] * (T(Union) << End) * TypeNoUnion[Rhs] >>
          [](Match& _) { return _(Lhs) << _(Rhs); },

        // Extend a right-side union type.
        TypeNoUnion[Lhs] * (T(Union) << End) * (T(Union) << Any)[Rhs] >>
          [](Match& _) { return Union << _(Lhs) << *_[Rhs]; },

        // Union type.
        TypeNoUnion[Lhs] * (T(Union) << End) * TypeNoUnion[Rhs] >>
          [](Match& _) { return Union << _(Lhs) << _(Rhs); },

        // A GlobalId in a wfType is a ClassId.
        In(
          Array,
          Ref,
          Cown,
          Union,
          FFIParams,
          Type,
          Field,
          Symbol,
          Func,
          Param,
          NewArray,
          NewArrayConst,
          StackArray,
          StackArrayConst,
          HeapArray,
          HeapArrayConst,
          RegionArray,
          RegionArrayConst,
          Typetest) *
            T(GlobalId)[GlobalId] >>
          [](Match& _) { return ClassId ^ _(GlobalId); },

        // Type grouping.
        // If it's prefixed with a string, it's an FFI parameter list.
        (!T(String))[Lhs] * T(LParen) * TypePat[Type] * T(RParen) >>
          [](Match& _) { return Seq << _(Lhs) << _(Type); },

        // Source file and offset.
        (T(Source) << End) * ~T(String)[String] * ~T(Int)[Int] >>
          [](Match& _) {
            Node seq = Seq;

            if (_(String))
              seq << (Source << _(String));

            if (_(Int))
              seq << (Offset << _(Int));

            return seq;
          },

        // Libraries.
        (T(Lib) << End) * ~T(String)[String] >>
          [](Match& _) {
            return Lib << (_(String) || (String ^ "")) << Symbols;
          },

        T(Lib)[Lib] * T(Symbol)[Symbol] >>
          [](Match& _) {
            (_(Lib) / Symbols) << _(Symbol);
            return _(Lib);
          },

        // FFI symbols.
        T(GlobalId)[GlobalId] * T(Equals) * T(String)[Lhs] * ~T(String)[Rhs] *
            SymbolParams * T(Colon) * TypePat[Type] >>
          [](Match& _) {
            return Symbol << (SymbolId ^ _(GlobalId)) << _(Lhs)
                          << (_(Rhs) || (String ^ "")) << (_(Vararg) || None)
                          << symbolparams(_[Params]) << _(Type);
          },

        // Type alias.
        (T(Type) << End) * T(GlobalId)[GlobalId] * T(Equals) * TypePat[Type] >>
          [](Match& _) { return Type << (TypeId ^ _(GlobalId)) << _(Type); },

        // Primitive class.
        (T(Primitive) << End) * PrimitiveType[Type] >>
          [](Match& _) { return Primitive << _(Type) << Methods; },

        (T(Primitive) << PrimitiveType)[Primitive] * T(GlobalId)[Lhs] *
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
            TypePat[Type] >>
          [](Match& _) {
            (_(Class) / Fields)
              << (Field << (FieldId ^ _(GlobalId)) << _(Type));
            return _(Class);
          },

        (T(Class) << T(ClassId))[Class] * T(GlobalId)[Lhs] * T(GlobalId)[Rhs] >>
          [](Match& _) {
            (_(Class) / Methods)
              << (Method << (MethodId ^ _(Lhs)) << (FunctionId ^ _(Rhs)));
            return _(Class);
          },

        // Function.
        (T(Func) << End) * T(GlobalId)[GlobalId] * ParamDef[Params] * T(Colon) *
            TypePat[Type] >>
          [](Match& _) {
            auto start = std::string(_(GlobalId)->location().view());
            start.at(0) = '^';
            return Seq << (Func << (FunctionId ^ _(GlobalId))
                                << paramdef(_[Params]) << _(Type) << Labels)
                       << (LabelId ^ start);
          },

        // Parameter.
        In(Group) * T(LocalId)[LocalId] * T(Colon) * TypePat[Type] >>
          [](Match& _) { return Param << _(LocalId) << _(Type); },

        // Globals.
        Dst * T(Global) * T(GlobalId)[GlobalId] >>
          [](Match& _) { return Global << _(LocalId) << _(GlobalId); },

        // Constants.
        Dst * T(Const) * T(None) >>
          [](Match& _) { return Const << _(LocalId) << None << None; },

        Dst * T(Const) * T(Bool) * T(True, False)[Rhs] >>
          [](Match& _) { return Const << _(LocalId) << Bool << _(Rhs); },

        Dst * T(Const) * IntType[Type] * IntLiteral[Rhs] >>
          [](Match& _) {
            auto r = check_literal(_(Type), _(Rhs));
            if (r)
              return r;

            return Const << _(LocalId) << _(Type) << _(Rhs);
          },

        Dst * T(Const) * FloatType[Type] * FloatLiteral[Rhs] >>
          [](Match& _) {
            auto r = check_literal(_(Type), _(Rhs));
            if (r)
              return r;

            return Const << _(LocalId) << _(Type) << _(Rhs);
          },

        Dst * T(Const) * T(String)[Rhs] >>
          [](Match& _) { return ConstStr << _(LocalId) << _(Rhs); },

        // Convert.
        Dst * T(Convert) * PrimitiveType[Type] * T(LocalId)[Rhs] >>
          [](Match& _) { return Convert << _(LocalId) << _(Type) << _(Rhs); },

        // Allocation.
        Dst * T(New) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return New << _(LocalId) << (ClassId ^ _(GlobalId))
                       << callargs(_[Args]);
          },

        Dst * T(Stack) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return Stack << _(LocalId) << (ClassId ^ _(GlobalId))
                         << callargs(_[Args]);
          },

        Dst * T(Heap) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] *
            CallArgs[Args] >>
          [](Match& _) {
            return Heap << _(LocalId) << _(Rhs) << (ClassId ^ _(GlobalId))
                        << callargs(_[Args]);
          },

        Dst * T(Region) * RegionType[Rhs] * T(GlobalId)[GlobalId] *
            CallArgs[Args] >>
          [](Match& _) {
            return Region << _(LocalId) << _(Rhs) << (ClassId ^ _(GlobalId))
                          << callargs(_[Args]);
          },

        Dst * T(New) * TypePat[Type] * ArrayDynArg >>
          [](Match& _) { return NewArray << _(LocalId) << _(Type) << _(Rhs); },

        Dst * T(New) * TypePat[Type] * ArrayConstArg >>
          [](Match& _) {
            auto r = check_literal(U64, _(Rhs));
            if (r)
              return r;

            return NewArrayConst << _(LocalId) << _(Type) << _(Rhs);
          },

        Dst * T(Stack) * TypePat[Type] * ArrayDynArg >>
          [](Match& _) {
            return StackArray << _(LocalId) << _(Type) << _(Rhs);
          },

        Dst * T(Stack) * TypePat[Type] * ArrayConstArg >>
          [](Match& _) {
            auto r = check_literal(U64, _(Rhs));
            if (r)
              return r;

            return StackArrayConst << _(LocalId) << _(Type) << _(Rhs);
          },

        Dst * T(Heap) * T(LocalId)[Lhs] * TypePat[Type] * ArrayDynArg >>
          [](Match& _) {
            return HeapArray << _(LocalId) << _(Lhs) << _(Type) << _(Rhs);
          },

        Dst * T(Heap) * T(LocalId)[Lhs] * TypePat[Type] * ArrayConstArg >>
          [](Match& _) {
            auto r = check_literal(U64, _(Rhs));
            if (r)
              return r;

            return HeapArrayConst << _(LocalId) << _(Lhs) << _(Type) << _(Rhs);
          },

        Dst * T(Region) * RegionType[Lhs] * TypePat[Type] * ArrayDynArg >>
          [](Match& _) {
            return RegionArray << _(LocalId) << _(Lhs) << _(Type) << _(Rhs);
          },

        Dst * T(Region) * RegionType[Lhs] * TypePat[Type] * ArrayConstArg >>
          [](Match& _) {
            auto r = check_literal(U64, _(Rhs));
            if (r)
              return r;

            return RegionArray << _(LocalId) << _(Lhs) << _(Type) << _(Rhs);
          },

        // Register operations.
        Dst * T(Copy) * T(LocalId)[Rhs] >>
          [](Match& _) { return Copy << _(LocalId) << _(Rhs); },

        Dst * T(Move) * T(LocalId)[Rhs] >>
          [](Match& _) { return Move << _(LocalId) << _(Rhs); },

        (T(Drop) << End) * T(LocalId)[LocalId] >>
          [](Match& _) { return Drop << _(LocalId); },

        // Reference operations.
        Dst * T(Ref) * T(LocalId)[LocalId] >>
          [](Match& _) { return RegisterRef << _(LocalId); },

        Dst * T(Ref) * T(LocalId)[Rhs] * T(GlobalId)[GlobalId] >>
          [](Match& _) {
            return FieldRef << _(LocalId) << (Arg << ArgCopy << _(Rhs))
                            << (FieldId ^ _(GlobalId));
          },

        Dst * T(Ref) * T(LocalId)[Lhs] * T(LocalId)[Rhs] >>
          [](Match& _) {
            return ArrayRef << _(LocalId) << (Arg << ArgCopy << _(Lhs))
                            << _(Rhs);
          },

        Dst * T(Ref) * T(LocalId)[Lhs] * IntLiteral[Rhs] >>
          [](Match& _) {
            auto r = check_literal(U64, _(Rhs));
            if (r)
              return r;

            return ArrayRefConst << _(LocalId) << (Arg << ArgCopy << _(Lhs))
                                 << _(Rhs);
          },

        Dst * T(Load) * T(LocalId)[Rhs] >>
          [](Match& _) { return Load << _(LocalId) << _(Rhs); },

        Dst * T(Store) * T(LocalId)[Lhs] * T(LocalId)[Rhs] >>
          [](Match& _) {
            return Store << _(LocalId) << _(Lhs) << (Arg << ArgCopy << _(Rhs));
          },

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
        Dst * T(Call) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return Call << _(LocalId) << (FunctionId ^ _(GlobalId))
                        << callargs(_[Args]);
          },

        // Dynamic call.
        Dst * T(Call) * T(LocalId)[Lhs] * CallArgs[Args] >>
          [](Match& _) {
            return CallDyn << _(LocalId) << _(Lhs) << callargs(_[Args]);
          },

        // Static subcall.
        Dst * T(Subcall) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return Subcall << _(LocalId) << (FunctionId ^ _(GlobalId))
                           << callargs(_[Args]);
          },

        // Dynamic subcall.
        Dst * T(Subcall) * T(LocalId)[Lhs] * CallArgs[Args] >>
          [](Match& _) {
            return SubcallDyn << _(LocalId) << _(Lhs) << callargs(_[Args]);
          },

        // Static try.
        Dst * T(Try) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return Try << _(LocalId) << (FunctionId ^ _(GlobalId))
                       << callargs(_[Args]);
          },

        // Dynamic try.
        Dst * T(Try) * T(LocalId)[Lhs] * CallArgs[Args] >>
          [](Match& _) {
            return TryDyn << _(LocalId) << _(Lhs) << callargs(_[Args]);
          },

        // FFI call.
        Dst * T(FFI) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return FFI << _(LocalId) << (SymbolId ^ _(GlobalId))
                       << callargs(_[Args]);
          },

        // When.
        Dst * T(When) * CallArgs[Args] * T(LocalId)[Rhs] >>
          [](Match& _) {
            return When << _(LocalId) << callargs(_[Args])
                        << (Arg << ArgCopy << _(Rhs));
          },

        // Type test.
        Dst * T(Typetest) * T(LocalId)[Rhs] * TypePat[Type] >>
          [](Match& _) { return Typetest << _(LocalId) << _(Rhs) << _(Type); },

        // Terminators.
        (T(Tailcall) << End) * T(GlobalId)[GlobalId] * CallArgs[Args] >>
          [](Match& _) {
            return Tailcall << (FunctionId ^ _(GlobalId)) << tailargs(_[Args]);
          },

        (T(Tailcall) << End) * T(LocalId)[Lhs] * CallArgs[Args] >>
          [](Match& _) { return TailcallDyn << _(Lhs) << tailargs(_[Args]); },

        (T(Return) << End) * T(LocalId)[LocalId] >>
          [](Match& _) { return Return << _(LocalId); },

        (T(Raise) << End) * T(LocalId)[LocalId] >>
          [](Match& _) { return Raise << _(LocalId); },

        (T(Throw) << End) * T(LocalId)[LocalId] >>
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
}
