#include "../lang.h"

namespace vc
{
  const std::initializer_list<Token> wfTypeElement = {
    None, Bool, I8,       I16,   I32,   I64,      U8,       U16,
    U32,  U64,  ILong,    ULong, ISize, USize,    F32,      F64,
    Ptr,  Dyn,  TypeName, Union, Isect, FuncType, TupleType};

  // TODO: remove as more expressions are handled.
  // everything from Const on isn't handled.
  const std::initializer_list<Token> wfExprElement = {
    ExprSeq, DontCare, Ident, None,     True,   False,     Bin,      Oct,
    Int,     Hex,      Float, HexFloat, String, RawString, DontCare, Const,
    Tuple,   Let,      Var,   Lambda,   QName,  Method,    Call,     CallDyn,
    If,      While,    For,   When,     Equals, Else,      Ref,      Try,
    Op,      Const,    Colon, Vararg};

  const auto FieldPat = T(Ident)[Ident] * ~(T(Colon) * (!T(Equals))++[Type]) *
    ~(T(Equals) * Any++[Body]);
  const auto TypeParamsPat = T(Bracket) << (T(List, Group) * End);
  const auto ParamsPat = T(Paren) << (~T(List, Group) * End);

  const auto NamedType =
    T(Ident) * ~TypeArgsPat * (T(DoubleColon) * T(Ident) * ~TypeArgsPat)++;
  const auto SomeType =
    T(TypeName, Union, Isect, TupleType, FuncType, NoArgType);

  Node make_typename(NodeRange r)
  {
    Node tn = TypeName;
    Node te;

    for (auto& n : r)
    {
      if (n == Ident)
        te = TypeElement << n << TypeArgs;
      else if (n == Bracket)
        (te / TypeArgs) << *n;
      else if (n == DoubleColon)
        tn << te;
    }

    tn << te;
    return tn;
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

  Node make_qname(NodeRange r)
  {
    Node qn = QName;
    Node qe;

    for (auto& n : r)
    {
      if (n->in({Ident, SymbolId}))
        qe = QElement << n << TypeArgs;
      else if (n == Bracket)
        (qe / TypeArgs) << *n;
      else if (n == DoubleColon)
        qn << qe;
    }

    qn << qe;
    return qn;
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
            return (ClassDef ^ _(Directory))
              << (Ident ^ _(Directory)->location()) << TypeParams
              << (ClassBody << (Group << Use << (Ident ^ "std") << DoubleColon
                                      << (Ident ^ "builtin"))
                            << *_[Directory]);
          },

        // Files in a directory aren't semantically meaningful.
        T(File)[File] >> [](Match& _) { return Seq << *_[File]; },

        // Class.
        In(ClassBody) * T(Group)
            << (T(Ident)[Ident] * ~TypeParamsPat[TypeParams] *
                T(Brace)[Brace]) >>
          [](Match& _) {
            return ClassDef << _(Ident) << (TypeParams << *_[TypeParams])
                            << (ClassBody << *_[Brace]);
          },

        // Field.
        In(ClassBody) * T(Group) << (FieldPat * End) >>
          [](Match& _) {
            return FieldDef << _(Ident) << (Type << _[Type])
                            << (Body << (Group << _[Body]));
          },

        // Function.
        In(ClassBody) * T(Group)
            << (T(Ident, SymbolId)[Ident] * ~TypeParamsPat[TypeParams] *
                ParamsPat[Params] * ~(T(Colon) * (!T(Brace))++[Type]) *
                T(Brace)[Brace]) >>
          [](Match& _) {
            return Function << _(Ident) << (TypeParams << *_[TypeParams])
                            << (Params << *_[Params]) << (Type << _[Type])
                            << (Body << *_[Brace]);
          },

        // Type alias.
        T(Group)[Group]
            << (T(Use) * T(Ident)[Ident] * ~TypeParamsPat[TypeParams] *
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
        In(Params) * (T(Group) << (FieldPat * End)) >>
          [](Match& _) {
            return ParamDef << _(Ident) << (Type << _[Type])
                            << (Body << (Group << _[Body]));
          },

        In(Params) * T(Group)[Group] >>
          [](Match& _) { return err(_(Group), "Expected a parameter"); },

        // Type parameters.
        T(TypeParams) << (T(List)[List] * End) >>
          [](Match& _) { return TypeParams << *_[List]; },

        In(TypeParams) * (T(Group) << (FieldPat * End)) >>
          [](Match& _) {
            return TypeParam << _(Ident) << (Type << _[Type])
                             << (Type << _[Body]);
          },

        In(TypeParams) * T(Group)[Group] >>
          [](Match& _) { return err(_(Group), "Expected a type parameter"); },

        // Type arguments.
        T(TypeArgs) << (T(List)[List] * End) >>
          [](Match& _) { return TypeArgs << *_[List]; },

        In(TypeArgs) * (T(Group) << (!T(Const) * Any++)[Type]) >>
          [](Match& _) { return Type << _[Type]; },

        In(TypeArgs) * (T(Group) << (T(Const) * Any++[Expr])) >>
          [](Match& _) { return Expr << _[Expr]; },

        // Types.
        // Type name.
        In(Type)++ * --In(TypeElement) * NamedType[Type] >>
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

        // Empty parentheses is the "no arguments" type.
        In(Type)++ * T(Paren) << End >>
          [](Match&) -> Node { return NoArgType; },

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
        // Let.
        In(Expr) * T(Let) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) { return Let << _(Ident) << (Type << _[Type]); },

        // Var.
        In(Expr) * T(Var) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) { return Var << _(Ident) << (Type << _[Type]); },

        // Lambda.
        In(Expr) * TypeParamsPat[TypeParams] * ParamsPat[Params] *
            ~(T(Colon) * (!T(SymbolId, "->"))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << (TypeParams << *_[TypeParams])
                          << (Params << *_[Params]) << (Type << _[Type])
                          << (Body << *_[Brace] << (Expr << _[Rhs]));
          },

        // Lambda without type parameters.
        In(Expr) * ParamsPat[Params] *
            ~(T(Colon) * (!T(SymbolId, "->"))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << TypeParams << (Params << *_[Params])
                          << (Type << _[Type])
                          << (Body << *_[Brace] << (Expr << _[Rhs]));
          },

        // Lambda with a single parameter.
        In(Expr) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(SymbolId, "->"))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << TypeParams
                          << (Params
                              << (ParamDef << _(Ident) << (Type << _[Type])
                                           << Body))
                          << Type << (Body << *_[Brace] << (Expr << _[Rhs]));
          },

        // Lambda without parameters.
        In(Expr) * T(Brace)[Brace] >>
          [](Match& _) {
            return Lambda << TypeParams << Params << Type
                          << (Body << *_[Brace]);
          },

        // Qualified name.
        In(Expr) *
            (T(Ident) * ~TypeArgsPat * T(DoubleColon) * T(Ident, SymbolId) *
             ~TypeArgsPat *
             (T(DoubleColon) * T(Ident, SymbolId) * ~TypeArgsPat)++)[QName] >>
          [](Match& _) { return make_qname(_[QName]); },

        // Unprefixed qualified name.
        // An identifier with type arguments is a qualified name.
        In(Expr) * T(Ident)[Ident] * TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return QName
              << (QElement << _(Ident) << (TypeArgs << *_[TypeArgs]));
          },

        // Method.
        In(Expr) * (T(Ident) / ApplyLhsPat)[Expr] * T(Dot) *
            T(Ident, SymbolId)[Ident] * ~TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return Method << (Expr << _(Expr)) << _(Ident)
                          << (TypeArgs << *_[TypeArgs]);
          },

        // Operator.
        In(Expr) * T(SymbolId)[SymbolId] * ~TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return Op << _(SymbolId) << (TypeArgs << *_[TypeArgs]);
          },

        // Static call.
        In(Expr) * T(QName)[QName] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) { return Call << _(QName) << seq_to_args(_(ExprSeq)); },

        // Dynamic call.
        In(Expr) * T(Method)[Method] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << _(Method) << seq_to_args(_(ExprSeq));
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

        // When.
        In(Expr) * (T(When) << End) * (!T(Lambda))++[For] * T(Lambda)[Lambda] >>
          [](Match& _) { return When << (Expr << _[For]) << _(Lambda); },

        // Groups are expressions.
        In(Body, Expr, ExprSeq, Tuple, Args) * T(Group)[Group] >>
          [](Match& _) { return Expr << *_[Group]; },

        // Parens are expression sequences.
        In(Body, Expr, Tuple) * T(Paren)[Paren] >>
          [](Match& _) { return ExprSeq << *_[Paren]; },

        // Lists are tuples.
        In(Body, ExprSeq, Tuple, Args) * T(List)[List] >>
          [](Match& _) -> Node { return Expr << (Tuple << *_[List]); },

        In(Expr) * T(List)[List] >>
          [](Match& _) -> Node { return Tuple << *_[List]; },

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
            if (!child->in(wfExprElement))
            {
              node->replace(child, err(child, "Expected an expression"));
              ok = false;
            }
          }
        }
        else if (node == Type)
        {
          if (node->size() > 1)
          {
            node->replace(
              node->front(), err(node->front(), "Expected a single type"));
            ok = false;
          }
          else
          {
            for (auto& child : *node)
            {
              if (!child->in(wfTypeElement))
              {
                node->replace(child, err(child, "Expected a type"));
                ok = false;
              }
            }
          }
        }
        else if (node == FuncType)
        {
          if (((node / Lhs) != NoArgType) && !(node / Lhs)->in(wfTypeElement))
          {
            node->replace(node->front(), err(node->front(), "Expected a type"));
            ok = false;
          }

          if (!(node / Rhs)->in(wfTypeElement))
          {
            node->replace(node->back(), err(node->back(), "Expected a type"));
            ok = false;
          }
        }
        else if (node->in({Union, Isect, TupleType}))
        {
          for (auto& child : *node)
          {
            if (!child->in(wfTypeElement))
            {
              node->replace(child, err(child, "Expected a type"));
              ok = false;
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
