#pragma once

#define TRIESTE_EXPOSE_LOG_MACRO
#include <trieste/trieste.h>
#include <vbcc.h>
#include <vbcc/lang.h>

namespace vc
{
  using namespace trieste;
  using namespace trieste::wf::ops;
  using namespace vbcc;

  inline const auto Paren = TokenDef("paren");
  inline const auto Bracket = TokenDef("bracket");
  inline const auto Brace = TokenDef("brace");
  inline const auto List = TokenDef("list");
  inline const auto DoubleColon = TokenDef("doublecolon");
  inline const auto TripleColon = TokenDef("triplecolon");
  inline const auto Dot = TokenDef("dot");
  inline const auto DontCare = TokenDef("dontcare");
  inline const auto Ident = TokenDef("ident", flag::print);
  inline const auto Use = TokenDef("use");
  inline const auto Shape = TokenDef("shape");
  inline const auto Hash = TokenDef("hash");

  inline const auto ClassDef = TokenDef(
    "classdef", flag::symtab | flag::lookup | flag::lookdown | flag::shadowing);
  inline const auto TypeAlias = TokenDef(
    "typealias",
    flag::symtab | flag::lookup | flag::lookdown | flag::shadowing);
  inline const auto FieldDef = TokenDef("fielddef");
  inline const auto Function =
    TokenDef("function", flag::symtab | flag::lookdown);
  inline const auto ParamDef =
    TokenDef("paramdef", flag::lookup | flag::shadowing);

  inline const auto TypeName = TokenDef("typename");
  inline const auto NameElement = TokenDef("nameelement");
  inline const auto TypePath = TokenDef("typepath");
  inline const auto TypeParams = TokenDef("typeparams");
  inline const auto TypeParam =
    TokenDef("typeparam", flag::lookup | flag::shadowing);
  inline const auto ValueParam = TokenDef("valueparam");
  inline const auto TypeArgs = TokenDef("typeargs");
  inline const auto Where = TokenDef("where");
  inline const auto ClassBody = TokenDef("classbody");

  inline const auto Isect = TokenDef("isect");
  inline const auto TupleType = TokenDef("tupletype");
  inline const auto FuncType = TokenDef("functype");
  inline const auto NoArgType = TokenDef("noargtype");
  inline const auto TypeVar = TokenDef("typevar", flag::print);

  inline const auto WhereAnd = TokenDef("whereand");
  inline const auto WhereOr = TokenDef("whereor");
  inline const auto WhereNot = TokenDef("wherenot");
  inline const auto SubType = TokenDef("subtype");

  inline const auto Expr = TokenDef("expr");
  inline const auto ExprSeq = TokenDef("exprseq");
  inline const auto Tuple = TokenDef("tuple");
  inline const auto TupleLHS = TokenDef("tuplelhs");
  inline const auto Lambda = TokenDef("lambda", flag::symtab);
  inline const auto Block = TokenDef("block", flag::symtab);
  inline const auto FuncName = TokenDef("funcname");
  inline const auto MethodName = TokenDef("methodname");
  inline const auto Binop = TokenDef("binop");
  inline const auto Unop = TokenDef("unop");
  inline const auto Nulop = TokenDef("nulop");
  inline const auto NewArgs = TokenDef("newargs");
  inline const auto NewArg = TokenDef("newarg");

  inline const auto Let = TokenDef("let", flag::lookup | flag::shadowing);
  inline const auto Var = TokenDef("var", flag::lookup | flag::shadowing);
  inline const auto If = TokenDef("if");
  inline const auto Else = TokenDef("else");
  inline const auto While = TokenDef("while");
  inline const auto For = TokenDef("for");
  inline const auto Break = TokenDef("break");
  inline const auto Continue = TokenDef("continue");

  const auto ValuePat =
    T(True,
      False,
      Bin,
      Oct,
      Int,
      Hex,
      Float,
      HexFloat,
      String,
      RawString,
      Convert,
      Binop,
      Unop,
      Nulop,
      FFI,
      NewArray,
      ArrayRef,
      FieldRef,
      Load,
      New,
      If,
      Else,
      While,
      For,
      When,
      Equals,
      LocalId,
      Call,
      CallDyn,
      Tuple,
      ExprSeq);

  inline const auto wfType =
    TypeName | Union | Isect | TupleType | FuncType | TypeVar;
  inline const auto wfWhere = WhereAnd | WhereOr | WhereNot | SubType;

  inline const auto wfBody =
    Use | Break | Continue | Return | Raise | Throw | Expr;

  inline const auto wfBinop = Add | Sub | Mul | Div | Mod | Pow | And | Or |
    Xor | Shl | Shr | Eq | Ne | Lt | Le | Gt | Ge | Min | Max | LogBase | Atan2;

  inline const auto wfUnop = Neg | Not | Abs | Ceil | Floor | Exp | Log | Sqrt |
    Cbrt | IsInf | IsNaN | Sin | Cos | Tan | Asin | Acos | Atan | Sinh | Cosh |
    Tanh | Asinh | Acosh | Atanh | Bits | Len | MakePtr | Read;

  inline const auto wfNulop = None | Const_E | Const_Pi | Const_Inf | Const_NaN;

  inline const auto wfExprStructure = ExprSeq | DontCare | TripleColon |
    wfLiteral | String | RawString | Tuple | Let | Var | New | Lambda | Ref |
    FuncName | Dot | If | Else | While | For | When | Equals | Hash | Try |
    FieldRef;

  inline const auto wfFuncLhs = Lhs >>= Lhs | Rhs;
  inline const auto wfFuncId = Ident >>= Ident | SymbolId;

  // clang-format off
  inline const auto wfPassStructure =
      (Top <<= ClassDef++)
    | (ClassDef <<=
        (Shape >>= Shape | None) * Ident * TypeParams * Where *
        ClassBody)[Ident]
    | (ClassBody <<= (ClassDef | Use | TypeAlias | Lib | FieldDef | Function)++)
    | (Lib <<= String * Symbols)
    | (Symbols <<= Symbol++)
    | (Symbol <<=
        SymbolId * (Lhs >>= String) * (Rhs >>= String) *
        (Vararg >>= Vararg | None) * FFIParams * Type)
    | (FFIParams <<= Type++)
    | (Use <<= TypeName)[Include]
    | (TypeAlias <<= Ident * TypeParams * Where * Type)[Ident]
    | (Where <<= ~wfWhere)
    | (FieldDef <<= Ident * Type)
    | (Function <<=
        wfFuncLhs * wfFuncId * TypeParams * Params * Type * Where * Body)[Ident]
    | (TypeName <<= NameElement++[1])
    | (NameElement <<= wfFuncId * TypeArgs)
    | (TypeParams <<= (TypeParam | ValueParam)++)
    | (TypeParam <<= Ident * Type)[Ident]
    | (ValueParam <<= Ident * Type * Body)[Ident]
    | (TypeArgs <<= (Type | Expr)++)
    | (Params <<= ParamDef++)
    | (ParamDef <<= Ident * Type * Body)[Ident]
    | (Type <<= wfType)
    | (Union <<= wfType++[2])
    | (Isect <<= wfType++[2])
    | (TupleType <<= wfType++[2])
    | (FuncType <<= (Lhs >>= wfType | NoArgType) * (Rhs >>= wfType))
    | (WhereAnd <<= wfWhere++[2])
    | (WhereOr <<= wfWhere++[2])
    | (WhereNot <<= wfWhere)
    | (SubType <<= (Lhs >>= Type) * (Rhs >>= Type))
    | (Body <<= wfBody++)
    | (Expr <<= wfExprStructure++)
    | (ExprSeq <<= Expr++)
    | (FieldRef <<= Expr * FieldId)
    | (Load <<= Expr)
    | (Tuple <<= Expr++[2])
    | (Lambda <<= Params * Type * Body)
    | (Block <<= Params * Type * Body)
    | (FuncName <<= NameElement++[1])
    | (TripleColon <<= NameElement++[1])
    | (Dot <<= wfFuncId * TypeArgs)
    | (If <<= Expr * Block)
    | (Else <<= Expr * Block)
    | (While <<= Expr * Block)
    | (For <<= Expr * Block)
    | (When <<= Expr * Type * Expr)
    | (Equals <<= (Lhs >>= Expr) * (Rhs >>= Expr))
    | (Let <<= Ident * Type)[Ident]
    | (Var <<= Ident * Type)[Ident]
    | (New <<= NewArgs)
    | (NewArgs <<= NewArg++)
    | (NewArg <<= Ident * Expr)
    | (Break <<= Expr)
    | (Continue <<= Expr)
    | (Return <<= Expr)
    | (Raise <<= Expr)
    | (Throw <<= Expr)
    ;
  // clang-format on

  inline const auto wfExprIdent = wfExprStructure | LocalId | MethodName;
  inline const auto wfBodyIdent = wfBody - Use;

  // clang-format off
  inline const auto wfPassIdent =
      wfPassStructure
    | (ClassBody <<= (ClassDef | TypeAlias | Lib | FieldDef | Function)++)
    | (Body <<= wfBodyIdent++)
    | (TypeName <<= NameElement++[1])
    | (FuncName <<= NameElement++[1])
    | (MethodName <<= wfFuncId * TypeArgs)
    | (Expr <<= wfExprIdent++)
    ;
  // clang-format on

  inline const auto wfExprSugar = (wfExprIdent | Load | Call) - Lambda;

  // clang-format off
  inline const auto wfPassSugar =
      wfPassIdent
    | (When <<= Args * Type * Expr)
    | (ParamDef <<= Ident * Type)[Ident]
    | (Call <<= FuncName * Args)
    | (Args <<= Expr++)
    | (Expr <<= wfExprSugar++)
    ;
  // clang-format on

  inline const auto wfExprDot = (wfExprSugar | CallDyn) - Dot;

  // clang-format off
  inline const auto wfPassDot =
      wfPassSugar
    | (CallDyn <<= Expr * wfFuncId * TypeArgs * Args)
    | (Expr <<= wfExprDot++)
    ;
  // clang-format on

  inline const auto wfExprApplication =
    (wfExprDot | Convert | Binop | Unop | Nulop | FFI | NewArray | ArrayRef |
     Ref | Hash) -
    FuncName - MethodName - TripleColon;

  // clang-format off
  inline const auto wfPassApplication =
      wfPassDot
    | (Expr <<= wfExprApplication)
    | (Convert <<= (Type >>= wfPrimitiveType) * Args)
    | (Binop <<= (MethodId >>= wfBinop) * Args)
    | (Unop <<= (MethodId >>= wfUnop) * Args)
    | (Nulop <<= (MethodId >>= wfNulop) * Args)
    | (FFI <<= SymbolId * Args)
    | (NewArray <<= Type * Args)
    | (ArrayRef <<= Args)
    | (Ref <<= Expr)
    | (Hash <<= Expr)
    ;
  // clang-format on

  inline const auto wfBodyANF = Const | ConstStr | Convert | Copy | Move |
    RegisterRef | FieldRef | ArrayRef | ArrayRefConst | New | NewArray |
    NewArrayConst | Load | Store | Lookup | Call | CallDyn | Typetest | Var |
    When | wfBinop | wfUnop | wfNulop | FFI;

  // clang-format off
  inline const auto wfPassANF =
      wfPassApplication
    | (Function <<=
        wfFuncLhs * wfFuncId * TypeParams * Params * Type * Where * Labels)
        [Ident]
    | (Labels <<= Label++)
    | (Label <<= LabelId * Body * (Return >>= wfTerminator))
    | (Body <<= wfBodyANF++)
    | (Const <<= wfDst * (Type >>= wfPrimitiveType) * (Rhs >>= wfLiteral))
    | (ConstStr <<= wfDst * String)
    | (Convert <<= wfDst * (Type >>= wfPrimitiveType) * wfSrc)
    | (Copy <<= wfDst * wfSrc)
    | (Move <<= wfDst * wfSrc)
    | (RegisterRef <<= wfDst * wfSrc)
    | (FieldRef <<= wfDst * Arg * FieldId)
    | (ArrayRef <<= wfDst * Arg * wfSrc)
    | (ArrayRefConst <<= wfDst * Arg * wfLit)
    | (New <<= wfDst * Type * NewArgs)
    | (NewArg <<= Ident * wfSrc)
    | (NewArray <<= wfDst * Type * wfSrc)
    | (NewArrayConst <<= wfDst * Type * wfLit)
    | (Load <<= wfDst * wfSrc)
    | (Store <<= wfDst * wfSrc * Arg)
    | (Lookup <<= wfDst * wfSrc * wfFuncLhs * wfFuncId * TypeArgs * Int)
    | (Call <<= wfDst * wfFuncLhs * FuncName * Args)
    | (CallDyn <<= wfDst * wfSrc * Args)
    | (Args <<= Arg++)
    | (Arg <<= (Type >>= (ArgMove | ArgCopy)) * wfSrc)
    | (Typetest <<= wfDst * wfSrc * Type)
    | (Return <<= LocalId)
    | (Raise <<= LocalId)
    | (Throw <<= LocalId)
    | (Cond <<= LocalId * (Lhs >>= LabelId) * (Rhs >>= LabelId))
    | (Jump <<= LabelId)
    | (When <<= wfDst * wfSrc * Args * Type)
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
    | (Bits <<= wfDst * wfSrc)
    | (Len <<= wfDst * wfSrc)
    | (Read <<= wfDst * wfSrc)
    | (FFI <<= wfDst * SymbolId * Args)
    ;
  // clang-format on

  inline const auto l_typevar = Location("typevar");

  Node make_type(Match& _, NodeRange r = {});
  Node make_typeargs(Node typeparams);
  Nodes scope_path(Node node);
  Node fq_typeparam(const Nodes& path, Node tp);
  Node fq_typeargs(const Nodes& path, Node tps);
  Node make_selftype(Node node, bool fq = false);

  Parse parser();
  PassDef structure(const Parse& parse);
  PassDef ident();
  PassDef sugar();
  PassDef dot();
  PassDef application();
  PassDef anf();
  PassDef reify();
}
