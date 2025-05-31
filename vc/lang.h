#pragma once

#include <trieste/trieste.h>
#include <vbcc.h>

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
  inline const auto If = TokenDef("if");
  inline const auto Else = TokenDef("else");
  inline const auto While = TokenDef("while");
  inline const auto For = TokenDef("for");

  inline const auto Let = TokenDef("let");
  inline const auto Var = TokenDef("var");
  inline const auto Break = TokenDef("break");
  inline const auto Continue = TokenDef("continue");

  Parse parser();
  PassDef structure();
}
