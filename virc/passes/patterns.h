#pragma once

#include <vir.h>

namespace virc
{
  using namespace trieste;
  using namespace vir;

  inline const auto Equals = TokenDef("=");
  inline const auto LParen = TokenDef("lparen");
  inline const auto RParen = TokenDef("rparen");
  inline const auto LBracket = TokenDef("lbracket");
  inline const auto RBracket = TokenDef("rbracket");
  inline const auto Comma = TokenDef(",");
  inline const auto Colon = TokenDef(":");

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
      Atanh,
      Bits,
      Len,
      MakePtr,
      Read);

  const auto Constant = T(Const_E, Const_Pi, Const_Inf, Const_NaN);

  const auto Def = Unop / Binop / Constant /
    T(Const,
      ConstStr,
      Convert,
      Singleton,
      New,
      Stack,
      Heap,
      Region,
      NewArray,
      NewArrayConst,
      StackArray,
      StackArrayConst,
      HeapArray,
      HeapArrayConst,
      RegionArray,
      RegionArrayConst,
      Copy,
      Move,
      RegisterRef,
      FieldRef,
      ArrayRef,
      ArrayRefConst,
      Load,
      Store,
      Lookup,
      Call,
      CallDyn,
      TryCallDyn,
      FFI,
      When,
      WhenDyn,
      GetRaise,
      SetRaise,
      Typetest,
      MakeCallback,
      CodePtrCallback,
      FreeCallback,
      Pin,
      Unpin,
      Merge,
      FFIStruct,
      FFILoad,
      FFIStore,
      AddExternal,
      RemoveExternal,
      Freeze,
      MemoSlot,
      ArrayCopy,
      ArrayFill,
      ArrayCompare);
}