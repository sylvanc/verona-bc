#pragma once

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
  inline const auto Dot = TokenDef("dot");
  inline const auto DontCare = TokenDef("dontcare");
  inline const auto Ident = TokenDef("ident", flag::print);
  inline const auto Use = TokenDef("use");

  inline const auto ClassDef = TokenDef(
    "classdef", flag::symtab | flag::lookup | flag::lookdown | flag::shadowing);
  inline const auto TypeAlias = TokenDef(
    "typealias",
    flag::symtab | flag::lookup | flag::lookdown | flag::shadowing);
  inline const auto FieldDef =
    TokenDef("fielddef", flag::lookdown | flag::shadowing);
  inline const auto Function =
    TokenDef("function", flag::symtab | flag::lookdown);
  inline const auto ParamDef =
    TokenDef("paramdef", flag::lookup | flag::shadowing);

  inline const auto TypeName = TokenDef("typename");
  inline const auto TypeElement = TokenDef("typeelement");
  inline const auto TypeParams = TokenDef("typeparams");
  inline const auto TypeParam =
    TokenDef("typeparam", flag::lookup | flag::shadowing);
  inline const auto TypeArgs = TokenDef("typeargs");
  inline const auto ClassBody = TokenDef("classbody");
  inline const auto RawString = TokenDef("rawstring", flag::print);

  inline const auto Isect = TokenDef("isect");
  inline const auto TupleType = TokenDef("tupletype");
  inline const auto FuncType = TokenDef("functype");
  inline const auto NoArgType = TokenDef("noargtype");

  inline const auto Expr = TokenDef("expr");
  inline const auto ExprSeq = TokenDef("exprseq");
  inline const auto Tuple = TokenDef("tuple");
  inline const auto Lambda = TokenDef("lambda", flag::symtab);
  inline const auto QName = TokenDef("qname");
  inline const auto QElement = TokenDef("qelement");
  inline const auto Op = TokenDef("op");
  inline const auto RefLet = TokenDef("reflet");
  inline const auto RefVar = TokenDef("refvar");
  inline const auto Apply = TokenDef("apply");
  inline const auto Bind = TokenDef("bind");

  inline const auto Let = TokenDef("let", flag::lookup | flag::shadowing);
  inline const auto Var = TokenDef("var", flag::lookup | flag::shadowing);
  inline const auto If = TokenDef("if");
  inline const auto Else = TokenDef("else");
  inline const auto While = TokenDef("while");
  inline const auto For = TokenDef("for");
  inline const auto Break = TokenDef("break");
  inline const auto Continue = TokenDef("continue");

  inline const auto TypeArgsPat = T(Bracket) << (T(List, Group) * End);

  inline const auto LiteralPat = T(
    None, True, False, Bin, Oct, Int, Hex, Float, HexFloat, String, RawString);

  inline const auto ApplyPat = LiteralPat /
    T(ExprSeq, RefLet, RefVar, Tuple, QName, Method, Call, CallDyn);

  inline const auto ExprPat =
    ApplyPat / T(DontCare, Lambda, If, While, For, When, Apply);

  inline const auto AssignPat = ExprPat / T(Let, Var);

  inline const auto wfType =
    TypeName | Union | Isect | TupleType | FuncType | NoArgType;

  inline const auto wfBody =
    Use | TypeAlias | Break | Continue | Return | Raise | Throw | Expr;

  inline const auto wfWeakExpr = Equals | Else | Ref | Try;

  // TODO: temporary placeholder.
  inline const auto wfTempExpr = Const | Colon | Vararg;

  inline const auto wfExpr = ExprSeq | DontCare | Ident | wfLiteral | String |
    RawString | Tuple | Let | Var | Lambda | QName | Op | Method | Call |
    CallDyn | If | While | For | When | wfWeakExpr | wfTempExpr;

  inline const auto wfFuncId = Ident >>= Ident | SymbolId;

  // clang-format off
  inline const auto wfPassStructure =
      (Top <<= ClassDef++)
    | (ClassDef <<= Ident * TypeParams * ClassBody)[Ident]
    | (ClassBody <<= (ClassDef | Use | TypeAlias | FieldDef | Function)++)
    | (Use <<= TypeName)[Include]
    | (TypeAlias <<= Ident * TypeParams * Type)[Ident]
    | (FieldDef <<= Ident * Type * Body)[Ident]
    | (Function <<= wfFuncId * TypeParams * Params * Type * Body)[Ident]
    | (TypeName <<= TypeElement++[1])
    | (TypeElement <<= Ident * TypeArgs)
    | (TypeParams <<= TypeParam++)
    | (TypeParam <<= Ident * (Lhs >>= Type) * (Rhs >>= Type))[Ident]
    | (TypeArgs <<= (Type | Expr)++)
    | (Params <<= ParamDef++)
    | (ParamDef <<= Ident * Type * Body)[Ident]
    | (Type <<= ~wfType)
    | (Union <<= wfType++[2])
    | (Isect <<= wfType++[2])
    | (TupleType <<= wfType++[2])
    | (FuncType <<= (Lhs >>= wfType) * (Rhs >>= wfType))
    | (Body <<= wfBody++)
    | (Expr <<= wfExpr++)
    | (ExprSeq <<= Expr++)
    | (Tuple <<= Expr++[2])
    | (Lambda <<= TypeParams * Params * Type * Body)
    | (QName <<= QElement++[1])
    | (QElement <<= wfFuncId * TypeArgs)
    | (Op <<= SymbolId * TypeArgs)
    | (Method <<= Expr * wfFuncId * TypeArgs)
    | (Call <<= QName * Args)
    | (CallDyn <<= Method * Args)
    | (Args <<= Expr++)
    | (If <<= Expr * Lambda)
    | (While <<= Expr * Lambda)
    | (For <<= Expr * Lambda)
    | (When <<= Expr * Lambda)
    | (Let <<= Ident * Type)[Ident]
    | (Var <<= Ident * Type)[Ident]
    | (Return <<= ~Expr)
    | (Raise <<= ~Expr)
    | (Throw <<= ~Expr)
    ;
  // clang-format on

  inline const auto wfExpr2 = (wfExpr | RefLet | RefVar | Apply) - Ident;

  // clang-format off
  inline const auto wfPassApplication =
      wfPassStructure
    | (Expr <<= wfExpr2++)
    | (RefLet <<= Ident)
    | (RefVar <<= Ident)
    | (Apply <<= Expr++[2])
    ;
  // clang-format on

  inline const auto wfExpr3 = wfExpr2 - (Op | Apply);

  // clang-format off
  inline const auto wfPassOperators =
      wfPassApplication
    | (Expr <<= wfExpr3)
    | (Ref <<= Expr)
    | (Try <<= Expr)
    | (Else <<= (Lhs >>= Expr) * (Rhs >>= Expr))
    | (Equals <<= (Lhs >>= Expr) * (Rhs >>= Expr))
    ;
  // clang-format on

  // clang-format off
  inline const auto wfPassANF =
      wfPassOperators
    ;
  // clang-format on

  Node seq_to_args(Node seq);

  Parse parser();
  PassDef structure();
  PassDef application();
  PassDef operators();
  PassDef anf();
}
