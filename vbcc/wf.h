#pragma once

#include "lang.h"

namespace vbcc
{
  using namespace trieste::wf::ops;

  inline const auto wfParserTokens = Primitive | Class | Func | GlobalId |
    LocalId | LabelId | Equals | LParen | RParen | Comma | None | Bool | I8 |
    I16 | I32 | I64 | U8 | U16 | U32 | U64 | F32 | F64 | Global | Const |
    Stack | Heap | Region | Copy | Move | Drop | Ref | Load | Store | Lookup |
    Arg | Call | Tailcall | Return | Cond | Jump | Add | Sub | Mul | Div | Mod |
    And | Or | Xor | Shl | Shr | Eq | Ne | Lt | Le | Gt | Ge | Min | Max |
    LogBase | Atan2 | Neg | Not | Abs | Ceil | Floor | Exp | Log | Sqrt | Cbrt |
    IsInf | IsNaN | Sin | Cos | Atan | Sinh | Cosh | Tanh | Asinh | Acosh |
    Atanh | Const_E | Const_Pi | Const_Inf | Const_NaN | True | False | Bin |
    Oct | Hex | Int | Float | HexFloat;

  // clang-format off
  inline const auto wfParser =
      (Top <<= (Directory | File)++)
    | (Directory <<= (Directory | File)++)
    | (File <<= Group)
    | (Group <<= wfParserTokens++)
    ;
  // clang-format on
}
