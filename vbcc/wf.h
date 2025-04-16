#pragma once

#include "lang.h"

namespace vbcc
{
  using namespace trieste::wf::ops;

  inline const auto wfRegionType = RegionRC | RegionGC | RegionArena;

  inline const auto wfPrimitiveType =
    None | Bool | I8 | I16 | I32 | I64 | U8 | U16 | U32 | U64 | F32 | F64;

  inline const auto wfLiteral =
    None | True | False | Bin | Oct | Hex | Int | Float | HexFloat;

  inline const auto wfBinop = Add | Sub | Mul | Div | Mod | And | Or | Xor |
    Shl | Shr | Eq | Ne | Lt | Le | Gt | Ge | Min | Max | LogBase | Atan2;

  inline const auto wfUnop = Neg | Not | Abs | Ceil | Floor | Exp | Log | Sqrt |
    Cbrt | IsInf | IsNaN | Sin | Cos | Atan | Sinh | Cosh | Tanh | Asinh |
    Acosh | Atanh;

  inline const auto wfConst = Const_E | Const_Pi | Const_Inf | Const_NaN;

  inline const auto wfStatement = Global | Const | Stack | Heap | Region |
    Copy | Move | Drop | Ref | Load | Store | Lookup | Arg | Call | wfBinop |
    wfUnop | wfConst;

  inline const auto wfTerminator =
    Tailcall | Return | Raise | Throw | Cond | Jump;

  inline const auto wfParserTokens = Primitive | Class | Func | GlobalId |
    LocalId | LabelId | Equals | LParen | RParen | Comma | Colon |
    wfRegionType | wfPrimitiveType | wfStatement | wfTerminator | wfBinop |
    wfUnop | wfConst | wfLiteral;

  // clang-format off
  inline const auto wfParser =
      (Top <<= (Directory | File)++)
    | (Directory <<= (Directory | File)++)
    | (File <<= Group)
    | (Group <<= wfParserTokens++)
    ;
  // clang-format on

  inline const auto wfDst = (LocalId >>= LocalId);
  inline const auto wfSrc = (Rhs >>= LocalId);
  inline const auto wfLhs = (Lhs >>= LocalId);
  inline const auto wfRhs = (Rhs >>= LocalId);

  // clang-format off
  inline const auto wfPassStatements =
      (Top <<=
        (Primitive | Class | Func | LabelId | wfStatement | wfTerminator)++)
    | (Type <<= wfPrimitiveType)
    | (Primitive <<= Type * Methods)
    | (Class <<= GlobalId * Fields * Methods)
    | (Fields <<= Field++)
    | (Field <<= GlobalId * Type)
    | (Methods <<= Method++)
    | (Method <<= (Lhs >>= GlobalId) * (Rhs >>= GlobalId))
    | (Func <<= GlobalId * Params * Type * Labels)
    | (Params <<= Param++)
    | (Param <<= LocalId * Type)
    | (Global <<= wfDst * GlobalId)
    | (Const <<= wfDst * Type * (Rhs >>= wfLiteral))
    | (Convert <<= wfDst * Type * (Rhs >>= LocalId))
    | (Stack <<= wfDst * GlobalId)
    | (Heap <<= wfDst * wfSrc * GlobalId)
    | (Region <<= wfDst * (Type >>= wfRegionType) * GlobalId)
    | (Copy <<= wfDst * wfSrc)
    | (Move <<= wfDst * wfSrc)
    | (Drop <<= LocalId)
    | (Ref <<= wfDst * wfSrc * GlobalId)
    | (Load <<= wfDst * wfSrc)
    | (Store <<= wfDst * wfLhs * wfRhs)
    | (Lookup <<= wfDst * (Rhs >>= (LocalId | None)) * (Func >>= GlobalId))
    | (Call <<= wfDst * (Func >>= (GlobalId | LocalId)) * Args)
    | (Args <<= Arg++)
    | (Arg <<= (Type >>= (ArgMove | ArgCopy)) * (Rhs >>= LocalId))
    | (Tailcall <<= (Func >>= (GlobalId | LocalId)) * Args)
    | (Return <<= LocalId)
    | (Raise <<= LocalId)
    | (Throw <<= LocalId)
    | (Cond <<= LocalId * (Lhs >>= LabelId) * (Rhs >>= LabelId))
    | (Jump <<= LabelId)
    | (Add <<= wfDst * wfLhs * wfRhs)
    | (Sub <<= wfDst * wfLhs * wfRhs)
    | (Mul <<= wfDst * wfLhs * wfRhs)
    | (Div <<= wfDst * wfLhs * wfRhs)
    | (Mod <<= wfDst * wfLhs * wfRhs)
    | (Pow <<= wfDst * wfLhs * wfRhs)
    | (And <<= wfDst * wfLhs * wfRhs)
    | (Or <<= wfDst * wfLhs * wfRhs)
    | (Xor <<= wfDst * wfLhs * wfRhs)
    | (Shl <<= wfDst * wfLhs * wfRhs)
    | (Shr <<= wfDst * wfLhs * wfRhs)
    | (Eq <<= wfDst * wfLhs * wfRhs)
    | (Ne <<= wfDst * wfLhs * wfRhs)
    | (Lt <<= wfDst * wfLhs * wfRhs)
    | (Le <<= wfDst * wfLhs * wfRhs)
    | (Gt <<= wfDst * wfLhs * wfRhs)
    | (Ge <<= wfDst * wfLhs * wfRhs)
    | (Min <<= wfDst * wfLhs * wfRhs)
    | (Max <<= wfDst * wfLhs * wfRhs)
    | (LogBase <<= wfDst * wfLhs * wfRhs)
    | (Atan2 <<= wfDst * wfLhs * wfRhs)
    | (Neg <<= wfDst * wfSrc)
    | (Not <<= wfDst * wfSrc)
    | (Abs <<= wfDst * wfSrc)
    | (Ceil <<= wfDst * wfSrc)
    | (Floor <<= wfDst * wfSrc)
    | (Exp <<= wfDst * wfSrc)
    | (Log <<= wfDst * wfSrc)
    | (Sqrt <<= wfDst * wfSrc)
    | (Cbrt <<= wfDst * wfSrc)
    | (IsInf <<= wfDst * wfSrc)
    | (IsNaN <<= wfDst * wfSrc)
    | (Sin <<= wfDst * wfSrc)
    | (Cos <<= wfDst * wfSrc)
    | (Tan <<= wfDst * wfSrc)
    | (Asin <<= wfDst * wfSrc)
    | (Acos <<= wfDst * wfSrc)
    | (Atan <<= wfDst * wfSrc)
    | (Sinh <<= wfDst * wfSrc)
    | (Cosh <<= wfDst * wfSrc)
    | (Tanh <<= wfDst * wfSrc)
    | (Asinh <<= wfDst * wfSrc)
    | (Acosh <<= wfDst * wfSrc)
    | (Atanh <<= wfDst * wfSrc)
    | (Const_E <<= wfDst)
    | (Const_Pi <<= wfDst)
    | (Const_Inf <<= wfDst)
    | (Const_NaN <<= wfDst)
    ;
  // clang-format on

  // clang-format off
  inline const auto wfPassLabels =
      wfPassStatements
    | (Top <<= (Primitive | Class | Func)++)
    | (Labels <<= Label++)
    | (Label <<= LabelId * Body * (Return >>= wfTerminator))
    | (Body <<= wfStatement++)
    ;
  // clang-format on
}
