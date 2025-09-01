#include "../lang.h"

namespace vc
{
  const std::initializer_list<Token> wfTypeElement = {
    TypeName, Union, Isect, FuncType, TupleType, RefType};

  const std::initializer_list<Token> wfExprElement = {
    ExprSeq, DontCare, Ident,    True,   False,     Bin,      Oct,     Int,
    Hex,     Float,    HexFloat, String, RawString, DontCare, Tuple,   Let,
    Var,     New,      Lambda,   QName,  Method,    Call,     CallDyn, If,
    While,   For,      When,     Equals, Else,      Ref,      Try,     Op,
    Convert, Binop,    Unop,     Nulop,  FieldRef,  Load};

  const auto FieldPat = T(Ident)[Ident] * ~(T(Colon) * (!T(Equals))++[Type]) *
    ~(T(Equals) * Any++[Body]);
  const auto TypeParamsPat = T(Bracket) << (T(List, Group) * End);
  const auto WherePat = T(Where) * (!T(Brace))++[Where];
  const auto ParamsPat = T(Paren) << (~T(List, Group) * End);
  const auto ElseLhsPat = (T(Else) << (T(Expr) * T(Block))) /
    (!T(Equals, Else) * (!T(Equals, Else))++);

  const auto NamedType =
    T(Ident) * ~TypeArgsPat * (T(DoubleColon) * T(Ident) * ~TypeArgsPat)++;

  const auto SomeType =
    T(TypeName,
      Union,
      Isect,
      RefType,
      TupleType,
      FuncType,
      NoArgType,
      SubType,
      WhereNot);

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

  Node lambda_body(Node brace, NodeRange rhs)
  {
    if (brace)
    {
      if (brace->empty())
        return Body << (Expr << (Ident ^ "none"));
      else
        return Body << *brace;
    }

    return Body << (Expr << rhs);
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
              << (Ident ^ _(Directory)->location()) << TypeParams << Where
              << (ClassBody << (Group << Use << (Ident ^ "std") << DoubleColon
                                      << (Ident ^ "builtin"))
                            << *_[Directory]);
          },

        // Files in a directory aren't semantically meaningful.
        T(File)[File] >> [](Match& _) { return Seq << *_[File]; },

        // Class.
        In(ClassBody) * T(Group)
            << (T(Ident)[Ident] * ~TypeParamsPat[TypeParams] * ~WherePat *
                T(Brace)[Brace]) >>
          [](Match& _) {
            return ClassDef << _(Ident) << (TypeParams << *_[TypeParams])
                            << (Where << _[Where]) << (ClassBody << *_[Brace]);
          },

        // Field.
        In(ClassBody) * T(Group) << (FieldPat * End) >>
          [](Match& _) {
            auto type = clone(Type << _[Type]);
            Node reftype =
              _[Type].empty() ? Type : Type << (RefType << _[Type]);
            return Seq << (FieldDef << clone(_(Ident)) << type
                                    << (Body << (Group << _[Body])))
                       << (Function
                           << Lhs << clone(_(Ident)) << TypeParams
                           << (Params
                               << (ParamDef << (Ident ^ "self")
                                            << make_selftype(_(Ident)) << Body))
                           << reftype << Where
                           << (Body
                               << (Expr
                                   << (FieldRef << (Expr << (Ident ^ "self"))
                                                << (FieldId ^ _(Ident))))));
          },

        // Function.
        In(ClassBody) * T(Group)
            << (~T(Ref)[Ref] * T(Ident, SymbolId)[Ident] *
                ~TypeParamsPat[TypeParams] * ParamsPat[Params] *
                ~(T(Colon) * (!T(Where, Brace))++[Type]) * ~WherePat *
                T(Brace)[Brace]) >>
          [](Match& _) {
            Node side = _(Ref) ? Lhs : Rhs;
            Node body = Body;

            if (_(Brace)->empty())
              body << (Expr << (Ident ^ "none"));
            else
              body << *_[Brace];

            return Function << side << _(Ident)
                            << (TypeParams << *_[TypeParams])
                            << (Params << *_[Params]) << (Type << _[Type])
                            << (Where << _[Where]) << body;
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
            Node body = Body;

            if (!_[Body].empty())
              body << (Group << _[Body]);

            return ParamDef << _(Ident) << (Type << _[Type]) << body;
          },

        In(Params) * T(Group)[Group] >>
          [](Match& _) { return err(_(Group), "Expected a parameter"); },

        // Type parameters.
        T(TypeParams) << (T(List)[List] * End) >>
          [](Match& _) { return TypeParams << *_[List]; },

        In(TypeParams) * (T(Group) << (FieldPat * End)) >>
          [](Match& _) {
            if (!_[Type].empty())
              return ValueParam << _(Ident) << (Type << _[Type])
                                << (Body << (Group << _[Body]));

            return TypeParam << _(Ident) << (Type << _[Body]);
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
        In(Type, Where)++ * --In(TypeElement) * NamedType[Type] >>
          [](Match& _) { return make_typename(_[Type]); },

        // Union type.
        In(Type, Where)++ * SomeType[Lhs] * T(SymbolId, "\\|") *
            SomeType[Rhs] >>
          [](Match& _) { return merge_type(Union, _(Lhs), _(Rhs)); },

        // Intersection type.
        In(Type, Where)++ * SomeType[Lhs] * T(SymbolId, "&") * SomeType[Rhs] >>
          [](Match& _) { return merge_type(Isect, _(Lhs), _(Rhs)); },

        // Reference type.
        In(Type, Where)++ * T(Ref) * SomeType[Type] >>
          [](Match& _) { return RefType << _(Type); },

        // Tuple type.
        In(Type, Where)++ * T(List)[List] >>
          [](Match& _) { return TupleType << *_[List]; },

        // Tuple type element.
        In(TupleType) * T(Group) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Function types are right associative.
        In(Type, Where)++ * (SomeType / T(Paren))[Lhs] * T(SymbolId, "->") *
            SomeType[Rhs] >>
          [](Match& _) {
            if (_(Lhs) == FuncType)
            {
              return FuncType << (_(Lhs) / Lhs)
                              << (FuncType << (_(Lhs) / Rhs) << _(Rhs));
            }

            return FuncType << _(Lhs) << _(Rhs);
          },

        // Negation type.
        In(Type, Where)++ * T(SymbolId, "!") * SomeType[Type] >>
          [](Match& _) { return WhereNot << _(Type); },

        // Subtype.
        In(Type, Where)++ * (SomeType / T(Paren))[Lhs] * T(SymbolId, "<") *
            SomeType[Rhs] >>
          [](Match& _) {
            return SubType << (Type << _(Lhs)) << (Type << _(Rhs));
          },

        // Empty parentheses is the "no arguments" type.
        In(Type, Where)++ * T(Paren) << End >>
          [](Match&) -> Node { return NoArgType; },

        // Type grouping.
        In(Type, Where)++* T(Paren) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Type grouping element.
        In(Type, Where)++ * In(Paren) * T(Group) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Where clauses.
        In(Where, WhereAnd, WhereOr, WhereNot) * T(Isect)[Isect] >>
          [](Match& _) { return WhereAnd << *_(Isect); },

        In(Where, WhereAnd, WhereOr, WhereNot) * T(Union)[Union] >>
          [](Match& _) { return WhereOr << *_(Union); },

        // Terminators.
        // Break, continue, return, raise, throw.
        T(Expr)[Expr]
            << (T(Break, Continue, Return, Raise, Throw)[Break] * Any++[Rhs]) >>
          [](Match& _) -> Node {
          auto b = _(Break);
          auto e = Expr << (_[Rhs] || Ident ^ "none");

          if (_(Expr)->parent() != Body)
            return err(_(Expr), "Can't be used as an expression");

          return b << e;
        },

        // Builtins.
        In(Expr) * T(TripleColon) * T(Ident)[Ident] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) -> Node {
          auto id = _(Ident)->location().view();

          if (id == "convi8")
            return Convert << I8 << seq_to_args(_(ExprSeq));
          else if (id == "convi16")
            return Convert << I16 << seq_to_args(_(ExprSeq));
          else if (id == "convi32")
            return Convert << I32 << seq_to_args(_(ExprSeq));
          else if (id == "convi64")
            return Convert << I64 << seq_to_args(_(ExprSeq));
          else if (id == "convu8")
            return Convert << U8 << seq_to_args(_(ExprSeq));
          else if (id == "convu16")
            return Convert << U16 << seq_to_args(_(ExprSeq));
          else if (id == "convu32")
            return Convert << U32 << seq_to_args(_(ExprSeq));
          else if (id == "convu64")
            return Convert << U64 << seq_to_args(_(ExprSeq));
          else if (id == "convilong")
            return Convert << ILong << seq_to_args(_(ExprSeq));
          else if (id == "convulong")
            return Convert << ULong << seq_to_args(_(ExprSeq));
          else if (id == "convisize")
            return Convert << ISize << seq_to_args(_(ExprSeq));
          else if (id == "convusize")
            return Convert << USize << seq_to_args(_(ExprSeq));
          else if (id == "convf32")
            return Convert << F32 << seq_to_args(_(ExprSeq));
          else if (id == "convf64")
            return Convert << F64 << seq_to_args(_(ExprSeq));
          else if (id == "add")
            return Binop << Add << seq_to_args(_(ExprSeq));
          else if (id == "sub")
            return Binop << Sub << seq_to_args(_(ExprSeq));
          else if (id == "mul")
            return Binop << Mul << seq_to_args(_(ExprSeq));
          else if (id == "div")
            return Binop << Div << seq_to_args(_(ExprSeq));
          else if (id == "mod")
            return Binop << Mod << seq_to_args(_(ExprSeq));
          else if (id == "pow")
            return Binop << Pow << seq_to_args(_(ExprSeq));
          else if (id == "and")
            return Binop << And << seq_to_args(_(ExprSeq));
          else if (id == "or")
            return Binop << Or << seq_to_args(_(ExprSeq));
          else if (id == "xor")
            return Binop << Xor << seq_to_args(_(ExprSeq));
          else if (id == "shl")
            return Binop << Shl << seq_to_args(_(ExprSeq));
          else if (id == "shr")
            return Binop << Shr << seq_to_args(_(ExprSeq));
          else if (id == "eq")
            return Binop << Eq << seq_to_args(_(ExprSeq));
          else if (id == "ne")
            return Binop << Ne << seq_to_args(_(ExprSeq));
          else if (id == "lt")
            return Binop << Lt << seq_to_args(_(ExprSeq));
          else if (id == "le")
            return Binop << Le << seq_to_args(_(ExprSeq));
          else if (id == "gt")
            return Binop << Gt << seq_to_args(_(ExprSeq));
          else if (id == "ge")
            return Binop << Ge << seq_to_args(_(ExprSeq));
          else if (id == "min")
            return Binop << Min << seq_to_args(_(ExprSeq));
          else if (id == "max")
            return Binop << Max << seq_to_args(_(ExprSeq));
          else if (id == "logbase")
            return Binop << LogBase << seq_to_args(_(ExprSeq));
          else if (id == "atan2")
            return Binop << Atan2 << seq_to_args(_(ExprSeq));
          else if (id == "neg")
            return Unop << Neg << seq_to_args(_(ExprSeq));
          else if (id == "not")
            return Unop << Not << seq_to_args(_(ExprSeq));
          else if (id == "abs")
            return Unop << Abs << seq_to_args(_(ExprSeq));
          else if (id == "ceil")
            return Unop << Ceil << seq_to_args(_(ExprSeq));
          else if (id == "floor")
            return Unop << Floor << seq_to_args(_(ExprSeq));
          else if (id == "exp")
            return Unop << Exp << seq_to_args(_(ExprSeq));
          else if (id == "log")
            return Unop << Log << seq_to_args(_(ExprSeq));
          else if (id == "sqrt")
            return Unop << Sqrt << seq_to_args(_(ExprSeq));
          else if (id == "cbrt")
            return Unop << Cbrt << seq_to_args(_(ExprSeq));
          else if (id == "isinf")
            return Unop << IsInf << seq_to_args(_(ExprSeq));
          else if (id == "isnan")
            return Unop << IsNaN << seq_to_args(_(ExprSeq));
          else if (id == "sin")
            return Unop << Sin << seq_to_args(_(ExprSeq));
          else if (id == "cos")
            return Unop << Cos << seq_to_args(_(ExprSeq));
          else if (id == "tan")
            return Unop << Tan << seq_to_args(_(ExprSeq));
          else if (id == "asin")
            return Unop << Asin << seq_to_args(_(ExprSeq));
          else if (id == "acos")
            return Unop << Acos << seq_to_args(_(ExprSeq));
          else if (id == "atan")
            return Unop << Atan << seq_to_args(_(ExprSeq));
          else if (id == "sinh")
            return Unop << Sinh << seq_to_args(_(ExprSeq));
          else if (id == "cosh")
            return Unop << Cosh << seq_to_args(_(ExprSeq));
          else if (id == "tanh")
            return Unop << Tanh << seq_to_args(_(ExprSeq));
          else if (id == "asinh")
            return Unop << Asinh << seq_to_args(_(ExprSeq));
          else if (id == "acosh")
            return Unop << Acosh << seq_to_args(_(ExprSeq));
          else if (id == "atanh")
            return Unop << Atanh << seq_to_args(_(ExprSeq));
          else if (id == "none")
            return Nulop << None << seq_to_args(_(ExprSeq));
          else if (id == "e")
            return Nulop << Const_E << seq_to_args(_(ExprSeq));
          else if (id == "pi")
            return Nulop << Const_Pi << seq_to_args(_(ExprSeq));
          else if (id == "inf")
            return Nulop << Const_Inf << seq_to_args(_(ExprSeq));
          else if (id == "nan")
            return Nulop << Const_NaN << seq_to_args(_(ExprSeq));

          return NoChange;
        },

        // Expressions.
        // Let.
        In(Expr) * (T(Let) << End) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) { return Let << _(Ident) << (Type << _[Type]); },

        // Var.
        In(Expr) * (T(Var) << End) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) { return Var << _(Ident) << (Type << _[Type]); },

        // Lambda.
        In(Expr) * ParamsPat[Params] *
            ~(T(Colon) * (!T(SymbolId, "->"))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << (Params << *_[Params]) << (Type << _[Type])
                          << lambda_body(_(Brace), _[Rhs]);
          },

        // Lambda with a single parameter.
        In(Expr) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(SymbolId, "->"))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << (Params
                              << (ParamDef << _(Ident) << (Type << _[Type])
                                           << Body))
                          << Type << lambda_body(_(Brace), _[Rhs]);
          },

        // Lambda without parameters.
        In(Expr) * T(Brace)[Brace] >>
          [](Match& _) {
            return Lambda << Params << Type << lambda_body(_(Brace), _[Rhs]);
          },

        // Qualified name.
        In(Expr) *
            (T(Ident) * ~TypeArgsPat * T(DoubleColon) * T(Ident, SymbolId) *
             ~TypeArgsPat *
             (T(DoubleColon) * T(Ident, SymbolId) * ~TypeArgsPat)++)[QName] >>
          [](Match& _) {
            Node qn = QName;
            Node qe;

            for (auto& n : _[QName])
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
          },

        // Unprefixed qualified name.
        // An identifier with type arguments is a qualified name.
        In(Expr) * T(Ident)[Ident] * TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return QName
              << (QElement << _(Ident) << (TypeArgs << *_[TypeArgs]));
          },

        // Method.
        In(Expr) *
            (LiteralPat /
             T(Ident, QName, Method, ExprSeq, Convert, Binop, Unop, Nulop))
              [Lhs] *
            T(Dot) * T(Ident, SymbolId)[Ident] * ~TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return Method << (Expr << _(Lhs)) << _(Ident)
                          << (TypeArgs << *_[TypeArgs]);
          },

        // Operator.
        In(Expr) * T(SymbolId)[SymbolId] * ~TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return Op << _(SymbolId) << (TypeArgs << *_[TypeArgs]);
          },

        // If.
        In(Expr) * (T(If) << End) * (!T(Lambda))++[Expr] * T(Lambda)[Lambda] >>
          [](Match& _) {
            return If << (Expr << _[Expr]) << (Block << *_(Lambda));
          },

        // Else.
        In(Expr) * ElseLhsPat[Lhs] * (T(Else) << End) * T(Lambda)[Lambda] >>
          [](Match& _) {
            if (!(_(Lambda) / Params)->empty())
              return err(_(Lambda), "Else block can't have parameters");

            return Else << (Expr << _[Lhs]) << (Block << *_(Lambda));
          },

        In(Expr) * ElseLhsPat[Lhs] * (T(Else) << End) *
            (!T(Equals, Else) * (!T(Equals, Else))++)[Rhs] >>
          [](Match& _) -> Node {
          // If the right-hand side is a brace, do nothing until it's
          // transformed into a lambda.
          if (_(Rhs) == Brace)
            return NoChange;

          return Else << (Expr << _[Lhs])
                      << (Block << TypeParams << Params << Type << Where
                                << (Body << (Expr << _[Rhs])));
        },

        // While.
        In(Expr) * (T(While) << End) * (!T(Lambda))++[While] *
            T(Lambda)[Lambda] >>
          [](Match& _) {
            if (!(_(Lambda) / Params)->empty())
              return err(_(Lambda), "While loop can't have parameters");

            return While << (Expr << _[While]) << (Block << *_(Lambda));
          },

        // For.
        In(Expr) * (T(For) << End) * (!T(Lambda))++[For] * T(Lambda)[Lambda] >>
          [](Match& _) {
            return For << (Expr << _[For]) << (Block << *_(Lambda));
          },

        // When.
        In(Expr) * (T(When) << End) * (!T(Lambda))++[For] * T(Lambda)[Lambda] >>
          [](Match& _) {
            return When << (Expr << _[For]) << (Block << *_(Lambda));
          },

        // Assignment is right-associative.
        In(Expr) * (T(Equals) << (T(Expr)[Lhs] * T(Expr)[Rhs])) *
            (T(Equals) << End) * (!T(Equals) * (!T(Equals))++)[Expr] >>
          [](Match& _) {
            return Equals << _(Lhs)
                          << (Expr << (Equals << _(Rhs) << (Expr << _[Expr])));
          },

        In(Expr) * (!T(Equals) * (!T(Equals))++)[Lhs] * (T(Equals) << End) *
            (!T(Equals) * (!T(Equals))++)[Rhs] >>
          [](Match& _) {
            return Equals << (Expr << _[Lhs]) << (Expr << _[Rhs]);
          },

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
        else if (node == Group)
        {
          node->parent()->replace(node, err(node, "Syntax error"));
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
        else if (node->in({Union, Isect, RefType, TupleType}))
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
        else if (node->in({Where, WhereAnd, WhereOr, WhereNot}))
        {
          for (auto& child : *node)
          {
            if (!child->in({WhereAnd, WhereOr, WhereNot, SubType}))
            {
              node->replace(child, err(child, "Expected a constraint"));
              ok = false;
            }
          }
        }
        else if (node == TypeParams)
        {
          for (auto& child : *node)
          {
            if (!child->in({TypeParam, ValueParam}))
            {
              node->replace(
                child, err(child, "Expected a type or value parameter"));
              ok = false;
            }
          }
        }
        else if (node == TypeArgs)
        {
          for (auto& child : *node)
          {
            if (!child->in({Type, Expr}))
            {
              node->replace(child, err(child, "Expected a type argument"));
              ok = false;
            }
          }
        }
        else if (node == Params)
        {
          for (auto& child : *node)
          {
            if (!child->in({ParamDef, ValueParam}))
            {
              node->replace(child, err(child, "Expected a parameter"));
              ok = false;
            }
          }
        }
        else if (node->in({Else, Equals}))
        {
          if (node->size() != 2)
          {
            node->parent()->replace(
              node, err(node, "Expected a left and right side"));
            ok = false;
          }
        }
        else if (node == Binop)
        {
          if ((node / Args)->size() != 2)
          {
            node->replace(
              node->front(), err(node->front(), "Expected two arguments"));
            ok = false;
          }
        }
        else if (node == Unop)
        {
          if ((node / Args)->size() != 1)
          {
            node->replace(
              node->front(), err(node->front(), "Expected one argument"));
            ok = false;
          }
        }
        else if (node == Nulop)
        {
          if ((node / Args)->size() != 0)
          {
            node->replace(
              node->front(), err(node->front(), "Expected no arguments"));
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
