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

  inline const auto TypeAlias = TokenDef("typealias");
  inline const auto TypeName = TokenDef("typename");
  inline const auto TypeElement = TokenDef("typeelement");
  inline const auto TypeParams = TokenDef("typeparams");
  inline const auto TypeParam = TokenDef("typeparam");
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
  inline const auto Lambda = TokenDef("lambda");
  inline const auto QName = TokenDef("qname");
  inline const auto QElement = TokenDef("qelement");
  inline const auto StaticCall = TokenDef("staticcall");
  inline const auto DynamicCall = TokenDef("dynamiccall");
  inline const auto Apply = TokenDef("apply");

  inline const auto Let = TokenDef("let");
  inline const auto Var = TokenDef("var");
  inline const auto If = TokenDef("if");
  inline const auto Else = TokenDef("else");
  inline const auto While = TokenDef("while");
  inline const auto For = TokenDef("for");
  inline const auto Break = TokenDef("break");
  inline const auto Continue = TokenDef("continue");

  inline const auto ApplyDef =
    T(ExprSeq,
      Ident,
      None,
      True,
      False,
      Bin,
      Oct,
      Int,
      Hex,
      Float,
      HexFloat,
      String,
      RawString,
      Tuple,
      QName,
      Method,
      StaticCall,
      DynamicCall);

  inline const auto wfType =
    TypeName | Union | Isect | TupleType | FuncType | NoArgType;

  inline const auto wfBody =
    Use | TypeAlias | Break | Continue | Return | Raise | Throw | Expr;

  inline const auto wfWeakExpr = Equals | Else | Ref | Try | SymbolId | Bracket;

  // TODO: temporary placeholder.
  inline const auto wfTempExpr = Const | Colon | Vararg;

  inline const auto wfExpr = ExprSeq | DontCare | Ident | wfLiteral | String |
    RawString | Tuple | Let | Var | Lambda | QName | Method | StaticCall |
    DynamicCall | If | While | For | When | wfWeakExpr | wfTempExpr;

  // clang-format off
  inline const auto wfPassStructure =
      (Top <<= Class++)
    | (Class <<= Ident * TypeParams * ClassBody)
    | (ClassBody <<= (Class | Use | TypeAlias | Field | Func)++)
    | (Use <<= TypeName)
    | (TypeAlias <<= Ident * TypeParams * Type)
    | (Field <<= Ident * Type * Body)
    | (Func <<= Ident * TypeParams * Params * Type * Body)
    | (TypeName <<= TypeElement++)
    | (TypeElement <<= Ident * TypeArgs)
    | (TypeParams <<= TypeParam++)
    | (TypeParam <<= Ident * (Lhs >>= Type) * (Rhs >>= Type))
    | (TypeArgs <<= (Type | Expr)++)
    | (Params <<= Param++)
    | (Param <<= Ident * Type * Body)
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
    | (QName <<= QElement++)
    | (QElement <<= (Ident >>= Ident | SymbolId) * TypeArgs)
    | (Method <<= (Expr >>= wfExpr) * (Ident >>= Ident | SymbolId) * TypeArgs)
    | (StaticCall <<= QName * ExprSeq)
    | (DynamicCall <<= Method * ExprSeq)
    | (If <<= Expr * Lambda)
    | (While <<= Expr * Lambda)
    | (For <<= Expr * Lambda)
    | (When <<= Expr * Lambda)
    | (Let <<= Ident * Type)
    | (Var <<= Ident * Type)
    | (Return <<= ~Expr)
    | (Raise <<= ~Expr)
    | (Throw <<= ~Expr)
    ;
  // clang-format on

  // clang-format off
  inline const auto wfPassApplication =
      wfPassStructure
    | (Expr <<= (Apply | wfExpr)++)
    | (Apply <<= wfExpr++[2])
    ;
  // clang-format on

  Parse parser();
  PassDef structure();
  PassDef application();
}
