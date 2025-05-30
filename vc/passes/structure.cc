#include "../lang.h"

#include <vbcc/lang.h>

namespace vc
{
  const auto wfType = TypeNames | Union | Isect | FuncType | TupleType;

  // TODO: temporary placeholder.
  const auto wfParserTokens =
    // Symbols.
    Ident | DontCare | Equals | Const | Colon | Vararg | Dot |
    // Keywords.
    When;

  const auto wfBody =
    Use | TypeAlias | Break | Continue | Return | Raise | Throw | Expr;

  const auto wfExpr = Expr | Tuple | Lambda | If | While | For | QName |
    wfLiteral | String | RawString | wfParserTokens;

  // clang-format off
  const auto wfPassStructure =
      (Top <<= Class++)
    | (Class <<= Ident * TypeParams * ClassBody)
    | (ClassBody <<= (Class | Use | TypeAlias | Field | Func)++)
    | (Use <<= TypeNames)
    | (TypeAlias <<= Ident * TypeParams * Type)
    | (Field <<= Ident * Type * Body)
    | (Func <<= Ident * TypeParams * Params * Type * Body)
    | (TypeNames <<= TypeName++)
    | (TypeName <<= Ident * TypeArgs)
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
    | (Tuple <<= Expr++[2])
    | (Lambda <<= TypeParams * Params * Type * Body)
    | (QName <<= TypeNames * (Ident >>= (SymbolId | Ident)) * TypeArgs)
    | (If <<= Expr * Lambda)
    | (While <<= Expr * Lambda)
    | (For <<= Expr * Lambda)
    | (Return <<= ~Expr)
    | (Raise <<= ~Expr)
    | (Throw <<= ~Expr)
    ;
  // clang-format on

  const auto FieldDef = T(Ident)[Ident] * ~(T(Colon) * (!T(Equals))++[Type]) *
    ~(T(Equals) * Any++[Body]);
  const auto TypeParamsDef = T(Bracket) << (T(List, Group) * End);
  const auto TypeArgsDef = T(Bracket) << (T(List, Group) * End);
  const auto ParamsDef = T(Paren) << (~T(List, Group) * End);

  const auto NamedType =
    T(Ident) * ~TypeArgsDef * (T(DoubleColon) * T(Ident) * ~TypeArgsDef)++;
  const auto SomeType = T(TypeNames, Union, Isect, FuncType, TupleType);

  Node make_typename(NodeRange r)
  {
    Node tns = TypeNames;
    Node tn;

    for (auto& n : r)
    {
      if (n == Ident)
        tn = TypeName << n << TypeArgs;
      else if (n == Bracket)
        (tn / TypeArgs) << *n;
      else if (n == DoubleColon)
        tns << tn;
    }

    tns << tn;
    return tns;
  }

  Node merge_type(Token t, Node lhs, Node rhs)
  {
    if (lhs == t)
    {
      if (rhs == t)
        return lhs << *rhs;
      else
        return lhs << rhs;
    }
    else if (rhs == t)
    {
      return t << lhs << *rhs;
    }
    else
    {
      return t << lhs << rhs;
    }
  }

  PassDef structure()
  {
    PassDef p{
      "structure",
      wfPassStructure,
      dir::topdown,
      {
        // Treat a directory as a class.
        T(Directory)[Directory] >>
          [](Match& _) {
            return (Class ^ _(Directory))
              << (Ident ^ _(Directory)->location()) << TypeParams
              << (ClassBody << (Group << Use << (Ident ^ "std") << DoubleColon
                                      << (Ident ^ "builtin"))
                            << *_[Directory]);
          },

        // Files in a directory aren't semantically meaningful.
        T(File)[File] >> [](Match& _) { return Seq << *_[File]; },

        // Class.
        In(ClassBody) * T(Group)
            << (T(Ident)[Ident] * ~TypeParamsDef[TypeParams] *
                T(Brace)[Brace]) >>
          [](Match& _) {
            return Class << _(Ident) << (TypeParams << *_[TypeParams])
                         << (ClassBody << *_[Brace]);
          },

        // Field.
        In(ClassBody) * T(Group) << (FieldDef * End) >>
          [](Match& _) {
            return Field << _(Ident) << (Type << _[Type])
                         << (Body << (Group << _[Body]));
          },

        // Function.
        In(ClassBody) * T(Group)
            << (T(Ident)[Ident] * ~TypeParamsDef[TypeParams] *
                ParamsDef[Params] * ~(T(Colon) * (!T(Brace))++[Type]) *
                T(Brace)[Brace]) >>
          [](Match& _) {
            return Func << _(Ident) << (TypeParams << *_[TypeParams])
                        << (Params << *_[Params]) << (Type << _[Type])
                        << (Body << *_[Brace]);
          },

        // Type alias.
        T(Group)[Group]
            << (T(Use) * T(Ident)[Ident] * ~TypeParamsDef[TypeParams] *
                T(Equals) * Any++[Type]) >>
          [](Match& _) {
            if (!_(Group)->parent()->in({ClassBody, Body}))
              return err(_(Group), "Type alias can't be used as an expression");

            return TypeAlias << _(Ident) << (TypeParams << *_[TypeParams])
                             << (Type << _[Type]);
          },

        // Import.
        T(Group)[Group] << (T(Use) * NamedType[Type] * End) >>
          [](Match& _) {
            if (!_(Group)->parent()->in({ClassBody, Body}))
              return err(_(Group), "Import can't be used as an expression");

            return Use << make_typename(_[Type]);
          },

        // Produce an error for anything else in a class body.
        In(ClassBody) * T(Group)[Group] >>
          [](Match& _) {
            return err(_(Group), "Can't appear in a class body");
          },

        // Parameters.
        T(Params) << (T(List)[List] * End) >>
          [](Match& _) { return Params << *_[List]; },

        // Parameter.
        In(Params) * (T(Group) << (FieldDef * End)) >>
          [](Match& _) {
            return Param << _(Ident) << (Type << _[Type])
                         << (Body << (Group << _[Body]));
          },

        In(Params) * T(Group)[Group] >>
          [](Match& _) { return err(_(Group), "Expected a parameter"); },

        // Type parameters.
        T(TypeParams) << (T(List)[List] * End) >>
          [](Match& _) { return TypeParams << *_[List]; },

        In(TypeParams) * (T(Group) << (FieldDef * End)) >>
          [](Match& _) {
            return TypeParam << _(Ident) << (Type << _[Type])
                             << (Type << _[Body]);
          },

        In(TypeParams) * T(Group)[Group] >>
          [](Match& _) { return err(_(Group), "Expected a type parameter"); },

        // Type arguments.
        T(TypeArgs) << (T(List)[List] * End) >>
          [](Match& _) { return TypeArgs << *_[List]; },

        In(TypeArgs) * (T(Group) << (--T(Const) * Any++)[Type]) >>
          [](Match& _) { return Type << _[Type]; },

        In(TypeArgs) * (T(Group) << (T(Const) * Any++[Expr])) >>
          [](Match& _) { return Expr << _[Expr]; },

        // Types.
        // Type name.
        In(Type)++ * --In(TypeName) * NamedType[Type] >>
          [](Match& _) { return make_typename(_[Type]); },

        // Union type.
        In(Type)++ * SomeType[Lhs] * T(SymbolId, "\\|") * SomeType[Rhs] >>
          [](Match& _) { return merge_type(Union, _(Lhs), _(Rhs)); },

        // Intersection type.
        In(Type)++ * SomeType[Lhs] * T(SymbolId, "&") * SomeType[Rhs] >>
          [](Match& _) { return merge_type(Isect, _(Lhs), _(Rhs)); },

        // Tuple type.
        In(Type)++ * T(List)[List] >>
          [](Match& _) -> Node { return TupleType << *_[List]; },

        // Tuple type element.
        In(TupleType) * T(Group) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Function types are right associative.
        In(Type)++ * (SomeType / T(Paren))[Lhs] * T(SymbolId, "->") *
            SomeType[Rhs] >>
          [](Match& _) {
            if (_(Lhs) == FuncType)
            {
              return FuncType << (_(Lhs) / Lhs)
                              << (FuncType << (_(Lhs) / Rhs) << _(Rhs));
            }

            return FuncType << _(Lhs) << _(Rhs);
          },

        // Empty parentheses is unit, which is `none`.
        In(Type)++ * T(Paren) << End >> [](Match&) -> Node { return None; },

        // Type grouping.
        In(Type)++* T(Paren) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Type grouping element.
        In(Type)++ * In(Paren) * T(Group) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Statements.
        // Break, continue, return, raise, throw.
        T(Expr)[Expr]
            << (T(Break, Continue, Return, Raise, Throw)[Break] * Any++[Rhs]) >>
          [](Match& _) -> Node {
          auto b = _(Break);
          auto e = Expr << _[Rhs];

          if (b->in({Break, Continue}) && !e->empty())
            return err(b, "Break or continue can't have a value");

          if (_(Expr)->parent() != Body)
            return err(_(Expr), "Can't be used as an expression");

          return b << e;
        },

        // Expressions.
        // Lambda.
        In(Expr) * TypeParamsDef[TypeParams] * ParamsDef[Params] *
            ~(T(Colon) * (!T(Brace))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << (TypeParams << *_[TypeParams])
                          << (Params << *_[Params]) << (Type << _[Type])
                          << (Body << *_[Brace] << (Expr << _[Rhs]));
          },

        // Lambda without type parameters.
        In(Expr) * ParamsDef[Params] * ~(T(Colon) * (!T(Brace))++[Type]) *
            T(SymbolId, "->") * (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << TypeParams << (Params << *_[Params])
                          << (Type << _[Type])
                          << (Body << *_[Brace] << (Expr << _[Rhs]));
          },

        // Lambda without parameters.
        In(Expr) * T(Brace)[Brace] >>
          [](Match& _) {
            return Lambda << TypeParams << Params << Type
                          << (Body << *_[Brace]);
          },

        // If.
        In(Expr) * (T(If) << End) * (!T(Lambda))++[Expr] * T(Lambda)[Lambda] >>
          [](Match& _) { return If << (Expr << _[Expr]) << _(Lambda); },

        // While.
        In(Expr) * (T(While) << End) * (!T(Lambda))++[While] *
            T(Lambda)[Lambda] >>
          [](Match& _) {
            if (!(_(Lambda) / Params)->empty())
              return err(_(Lambda), "While loop can't have parameters");

            return While << (Expr << _[While]) << _(Lambda);
          },

        // For.
        In(Expr) * (T(For) << End) * (!T(Lambda))++[For] * T(Lambda)[Lambda] >>
          [](Match& _) { return For << (Expr << _[For]) << _(Lambda); },

        // Qualified name.
        In(Expr) * NamedType[TypeName] * T(DoubleColon) *
            T(Ident, SymbolId)[Ident] * ~TypeArgsDef[TypeArgs] >>
          [](Match& _) {
            return QName << make_typename(_[TypeName]) << _(Ident)
                         << (TypeArgs << *_[TypeArgs]);
          },

        // Unprefixed qualified name.
        // An identifier with type arguments is a qualified name.
        // A symbol, with or without type arguments, is a qualified name.
        In(Expr) *
            ((T(Ident, SymbolId)[Ident] * TypeArgsDef[TypeArgs]) /
             T(SymbolId)[Ident]) >>
          [](Match& _) {
            return QName << TypeNames << _(Ident) << (TypeArgs << *_[TypeArgs]);
          },

        // Groups and parens in bodies, expressions, and tuples are expressions.
        In(Body, Expr, Tuple) * T(Group, Paren)[Group] >>
          [](Match& _) { return Expr << *_[Group]; },

        // Lists in bodies, expressions, and tuples are tuples.
        In(Body, Expr, Tuple) * T(List)[List] >>
          [](Match& _) -> Node { return Expr << (Tuple << *_[List]); },

        // Compact expressions that contain a single expression.
        T(Expr) << (T(Expr)[Expr] * End) >> [](Match& _) { return _(Expr); },

        // Remove empty expressions.
        T(Expr) << End >> [](Match&) -> Node { return {}; },
      }};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node == Expr)
        {
          for (auto& child : *node)
          {
            if (!child->in({Expr,     Tuple,  Lambda,    If,       While,
                            For,      QName,  Ident,     True,     False,
                            Bin,      Oct,    Int,       Hex,      Float,
                            HexFloat, String, RawString, DontCare, Const,
                            Vararg,   Dot,    Equals,    Colon,    When}))
            {
              node->replace(child, err(child, "Expected an expression"));
              ok = false;
            }
          }
        }
        else if (node->in({Type, Union, Isect, FuncType, TupleType}))
        {
          if ((node == Type) && (node->size() > 1))
          {
            node->replace(
              node->front(), err(node->front(), "Expected a single type"));
            ok = false;
          }
          else
          {
            for (auto& child : *node)
            {
              if (!child->in({None,  Bool,     I8,       I16,       I32,
                              I64,   U8,       U16,      U32,       U64,
                              ILong, ULong,    ISize,    USize,     F32,
                              F64,   Ptr,      Dyn,      TypeNames, Union,
                              Isect, FuncType, TupleType}))
              {
                node->replace(child, err(child, "Expected a type"));
                ok = false;
              }
            }
          }
        }
        else if (node->in({Break, Continue}))
        {
          auto p = node->parent(Lambda);

          if (!p || !p->parent()->in({While, For}))
          {
            p = node->parent();
            p->replace(
              node, err(node, "Break or continue must be in a loop body"));
            ok = false;
          }
        }

        return ok;
      });

      return 0;
    });

    return p;
  }
}
