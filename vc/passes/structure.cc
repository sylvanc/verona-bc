#include "../dependency.h"
#include "../lang.h"

namespace vc
{
  const std::initializer_list<Token> wfTypeElement = {
    TypeName, Union, Isect, FuncType, TupleType, TypeVar, TypeSelf};

  const std::initializer_list<Token> wfExprElement = {
    ExprSeq,  DontCare,  SplatLet, SplatDontCare, True,     False,
    Bin,      Oct,       Int,      Hex,           Float,    HexFloat,
    String,   RawString, Char,     Tuple,         ArrayLit, Let,
    Var,      New,       Lambda,   Dot,           Ref,      FuncName,
    If,       While,     When,     Equals,        Else,     TripleColon,
    FieldRef, MatchExpr};

  const auto FieldPat = T(Ident)[Ident] * ~(T(Colon) * Any++[Type]);
  const auto TypeParamsPat = T(Bracket) << (T(List, Group) * End);
  const auto WherePat = T(Where) * (!T(Brace, Equals))++[Where];
  const auto ParamsPat = T(Paren) << (~T(List, Group) * End);
  const auto ParamPat = T(Ident)[Ident] * ~(T(Colon) * (!T(Equals))++[Type]) *
    ~(T(Equals) * Any++[Body]);
  const auto ElseLhsPat = (T(Else) << (T(Expr) * T(Block))) /
    (!T(Equals, Else) * (!T(Equals, Else))++);
  const auto TypeArgsPat = T(Bracket) << (T(List, Group) * End);

  const auto NamedType =
    T(Ident) * ~TypeArgsPat * (T(DoubleColon) * T(Ident) * ~TypeArgsPat)++;

  const auto SomeType =
    T(TypeName,
      Union,
      Isect,
      TupleType,
      FuncType,
      NoArgType,
      SubType,
      WhereNot,
      TypeSelf);

  Node make_typename(NodeRange r)
  {
    Node tn = TypeName;
    Node te;

    for (auto& n : r)
    {
      if (n == Ident)
        te = NameElement << n << TypeArgs;
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

      return lhs << rhs;
    }

    if (rhs == t)
      return t << lhs << *rhs;

    return t << lhs << rhs;
  }

  Node lambda_body(Node brace, NodeRange rhs)
  {
    if (brace && (brace == Brace))
    {
      if (brace->empty())
        return Body << (Expr << (Ident ^ "none"));

      return Body << *brace;
    }

    return Body << (Expr << rhs);
  }

  PassDef structure(const Parse& parse)
  {
    PassDef p{
      "structure",
      wfPassStructure,
      dir::topdown,
      {
        // An else group is merged into the preceding group, but only
        // when the preceding group contains a brace (no semicolon).
        // The brace doesn't have to be at the end: after a prior merge,
        // `} else if {} else` produces a group with two braces, and the
        // second else must still merge into it.
        (T(Group) << ((!T(Brace))++ * T(Brace)))[Lhs] *
            (T(Group) << (T(Else) * Any++[Rhs])) >>
          [](Match& _) { return Group << *_[Lhs] << Else << _[Rhs]; },

        // Treat a directory as a class.
        T(Directory)[Directory] >>
          [](Match& _) {
            return (ClassDef ^ _(Directory))
              << None << (Ident ^ _(Directory)) << TypeParams << Where
              << (ClassBody << (Group << Use << (Ident ^ "_builtin"))
                            << *_[Directory]);
          },

        // Files in a directory aren't semantically meaningful.
        T(File)[File] >> [](Match& _) { return Seq << *_[File]; },

        // Class.
        In(ClassBody) * T(Group)
            << (~T(Shape)[Shape] * T(Ident)[Ident] *
                ~TypeParamsPat[TypeParams] * ~WherePat * T(Brace)[Brace]) >>
          [](Match& _) {
            return ClassDef << (_[Shape] || None) << _(Ident)
                            << (TypeParams << *_[TypeParams])
                            << (Where << _[Where]) << (ClassBody << *_[Brace]);
          },

        // Field.
        In(ClassBody) * T(Group) << (FieldPat * End) >>
          [](Match& _) {
            auto id = _(Ident);
            auto type = make_type(_[Type]);
            auto self = make_selftype(id);

            return Seq << (FieldDef << id << type)
                       << (Function
                           << Lhs << clone(id) << TypeParams
                           << (Params
                               << (ParamDef << (Ident ^ "self") << self
                                            << Body))
                           << (Type
                               << (TypeName
                                   << (NameElement
                                       << (Ident ^ "ref")
                                       << (TypeArgs << clone(type)))))
                           << Where
                           << (Body
                               << (Expr
                                   << (FieldRef << (Expr << (Ident ^ "self"))
                                                << (FieldId ^ id)))));
          },

        // Function.
        In(ClassBody) * T(Group)
            << (~(T(Ident, "ref") / T(Ident, "once"))[Lhs] *
                T(Ident, SymbolId)[Ident] * ~TypeParamsPat[TypeParams] *
                ParamsPat[Params] * ~(T(Colon) * (!T(Where, Brace))++[Type]) *
                ~WherePat * ~T(Brace)[Brace]) >>
          [](Match& _) {
            Node side;
            if (!_(Lhs))
              side = Rhs;
            else if (_(Lhs)->location().view() == "once")
              side = Once;
            else
              side = Lhs;

            Node body = Body;
            auto brace = _(Brace);
            auto shape = (_(Ident)->parent(ClassDef) / Shape) == Shape;

            if (brace && shape)
            {
              return err(
                _(Ident), "Function implementations are not allowed in shapes");
            }

            if (!brace && !shape)
            {
              return err(
                _(Ident), "Function prototypes are only allowed in shapes");
            }

            auto params = Params << *_[Params];
            if ((side == Once) && !params->empty())
            {
              return err(_(Ident), "once functions must have no parameters");
            }

            if (!brace || brace->empty())
              body << (Expr << (Ident ^ "none"));
            else
              body << *_[Brace];

            return Function
              << side << _(Ident) << (TypeParams << *_[TypeParams]) << params
              << make_type(_[Type]) << (Where << _[Where]) << body;
          },

        // Dependency alias.
        T(Group)
            << (T(Use) * T(Ident)[Ident] * T(Equals) * T(String)[Lhs] *
                ~T(String)[Rhs] * ~T(String)[Directory]) >>
          [](Match& _) {
            return Use << _(Ident) << _(Lhs) << _(Rhs) << _(Directory);
          },

        // Dependency import.
        T(Group)
            << (T(Use) * T(String)[Lhs] * ~T(String)[Rhs] *
                ~T(String)[Directory]) >>
          [](Match& _) { return Use << _(Lhs) << _(Rhs) << _(Directory); },

        // Type alias.
        T(Group)
            << (T(Use) * T(Ident)[Ident] * ~TypeParamsPat[TypeParams] *
                ~WherePat * T(Equals) * (Any * Any++)[Type]) >>
          [](Match& _) {
            return TypeAlias << _(Ident) << (TypeParams << *_[TypeParams])
                             << (Where << _[Where]) << (Type << _[Type]);
          },

        // Import.
        T(Group) << (T(Use) * NamedType[Type] * End) >>
          [](Match& _) { return Use << make_typename(_[Type]); },

        // FFI library.
        T(Group) << (T(Use) * ~T(String)[String] * T(Brace)[Brace] * End) >>
          [](Match& _) {
            return Lib << (_[String] || (String ^ ""))
                       << (Symbols << *_[Brace]);
          },

        // FFI init function.
        In(Symbols) * T(Group)
            << (~T(Ident, "ref")[Lhs] * T(Ident, SymbolId)[Ident] *
                ~TypeParamsPat[TypeParams] * ParamsPat[Params] *
                ~(T(Colon) * (!T(Where, Brace))++[Type]) * ~WherePat *
                T(Brace)[Brace]) >>
          [](Match& _) {
            auto name = _(Ident)->location().view();

            if (name != "init")
            {
              return err(
                _(Ident), "Only 'init' functions are allowed in use blocks");
            }

            if (_(Lhs))
              return err(_(Ident), "init functions cannot be ref");

            Node body = Body;

            if (_(Brace)->empty())
              body << (Expr << (Ident ^ "none"));
            else
              body << *_[Brace];

            return Function << Rhs << _(Ident) << (TypeParams << *_[TypeParams])
                            << (Params << *_[Params]) << make_type(_[Type])
                            << (Where << _[Where]) << body;
          },

        // FFI symbol.
        In(Symbols) * T(Group)
            << (T(Ident)[Ident] * T(Equals) * T(String)[Lhs] * ~T(String)[Rhs] *
                ParamsPat[Params] * T(Colon) * (Any * Any++)[Type]) >>
          [](Match& _) {
            auto params = _(Params);
            Node ffiparams = FFIParams;
            Node vararg;

            if (!params->empty())
            {
              if (params->front() == Group)
              {
                // If it's a single element, put the contents in a type.
                ffiparams << (Type << *params->front());
              }
              else
              {
                // Otherwise, it's a list, add multiple types.
                assert(params->front() == List);

                for (auto& p : *params->front())
                  ffiparams << (Type << *p);
              }
            }

            if (
              !ffiparams->empty() && (ffiparams->back()->size() == 1) &&
              (ffiparams->back()->front() == Vararg))
            {
              // Check for `...` at the end of the types.
              ffiparams->pop_back();
              vararg = Vararg;
            }
            else
            {
              vararg = None;
            }

            return Symbol << (SymbolId ^ _(Ident)) << _(Lhs)
                          << (_[Rhs] || String ^ "") << vararg << ffiparams
                          << (Type << _[Type]);
          },

        // Parameters.
        T(Params) << (T(List)[List] * End) >>
          [](Match& _) { return Params << *_[List]; },

        // Parameter.
        In(Params) * (T(Group) << (ParamPat * End)) >>
          [](Match& _) {
            Node body = Body;

            if (!_[Body].empty())
              body << (Group << _[Body]);

            return ParamDef << _(Ident) << make_type(_[Type]) << body;
          },

        // If it's not a parameter, it might be a case value.
        In(Params) * T(Group)[Group] >>
          [](Match& _) { return Expr << *_(Group); },

        // Type parameters.
        T(TypeParams) << (T(List)[List] * End) >>
          [](Match& _) { return TypeParams << *_[List]; },

        In(TypeParams) * (T(Group) << (T(Ident)[Ident] * End)) >>
          [](Match& _) { return TypeParam << _(Ident); },

        // Type arguments.
        T(TypeArgs) << (T(List)[List] * End) >>
          [](Match& _) { return TypeArgs << *_[List]; },

        In(TypeArgs) * (T(Group) << (!T(Const) * Any++)[Type]) >>
          [](Match& _) { return Type << _[Type]; },

        In(TypeArgs) * (T(Group) << (T(Const) * (Any * Any++)[Expr])) >>
          [](Match& _) { return Expr << _[Expr]; },

        // Types.
        // Self type (only valid inside shapes).
        In(Type, Where)++ * --In(NameElement) * T(Ident, "self")[Ident] >>
          [](Match& _) -> Node {
          auto cls = _(Ident)->parent({ClassDef});

          if (cls && ((cls / Shape) == Shape))
            return TypeSelf;

          return err(_(Ident), "`self` type is only valid in shapes");
        },

        // Type name.
        In(Type, Where)++ * --In(NameElement) * NamedType[Type] >>
          [](Match& _) { return make_typename(_[Type]); },

        // Union type.
        In(Type, Where)++* SomeType[Lhs] * T(SymbolId, "\\|") * SomeType[Rhs] *
            --T(SymbolId, "->") >>
          [](Match& _) { return merge_type(Union, _(Lhs), _(Rhs)); },

        // Intersection type.
        In(Type, Where)++* SomeType[Lhs] * T(SymbolId, "&") * SomeType[Rhs] *
            --T(SymbolId, "->") >>
          [](Match& _) { return merge_type(Isect, _(Lhs), _(Rhs)); },

        // Tuple type.
        In(Type, Where)++* T(List)[List] >>
          [](Match& _) { return TupleType << *_[List]; },

        // Tuple type element.
        In(TupleType) * T(Group) << (SomeType[Type] * End) >>
          [](Match& _) { return _(Type); },

        // Function types are right associative.
        In(Type, Where)++ * SomeType[Lhs] * T(SymbolId, "->") * SomeType[Rhs] >>
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
        // Break, continue, return, raise.
        T(Expr)[Expr]
            << (T(Break, Continue, Return, Raise)[Break] * Any++[Rhs]) >>
          [](Match& _) -> Node {
          auto b = _(Break);
          auto e = Expr << (_[Rhs] || Ident ^ "none");

          if (_(Expr)->parent() != Body)
            return err(_(Expr), "Can't be used as an expression");

          if (b == Raise)
          {
            auto ancestor = _(Expr)->parent({Lambda, Function});

            if (!ancestor || (ancestor == Function))
              return err(_(Expr), "Raise can only be used inside a lambda");
          }

          return b << e;
        },

        // Expressions.
        // Splat let: let name...
        In(Expr) * (T(Let) << End) * T(Ident)[Ident] * T(Vararg) *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) {
            if (!_[Type].empty())
              return err(_(Ident), "Splat let cannot have a type annotation");
            return Node{SplatLet << _(Ident)};
          },

        // Let.
        In(Expr) * (T(Let) << End) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) { return Let << _(Ident) << make_type(_[Type]); },

        // Var.
        In(Expr) * (T(Var) << End) * T(Ident)[Ident] *
            ~(T(Colon) * (!T(Equals))++[Type]) >>
          [](Match& _) { return Var << _(Ident) << make_type(_[Type]); },

        // Splat don't-care: _...
        In(Expr) * T(DontCare) * T(Vararg) >>
          [](Match&) -> Node { return SplatDontCare; },

        // New.
        In(Expr) * (T(New)[New] << End) *
            (T(Brace)[Brace] << (~T(List, Group) * End)) >>
          [](Match& _) {
            if ((_(New)->parent(ClassDef) / Shape) == Shape)
              return err(_(New), "Can't instantiate a shape");

            return New << (NewArgs << *_(Brace));
          },

        T(NewArgs) << (T(List)[List] * End) >>
          [](Match& _) { return NewArgs << *_(List); },

        In(NewArgs) *
            (T(Group) << (T(Ident)[Ident] * T(Equals) * Any++[Expr] * End)) >>
          [](Match& _) { return NewArg << _(Ident) << (Expr << _[Expr]); },

        // Shorthand field initializer: `new { foo }` desugars to
        // `new { foo = foo }`.
        In(NewArgs) * (T(Group) << (T(Ident)[Ident] * End)) >>
          [](Match& _) {
            auto id = _(Ident);
            return NewArg << clone(id) << (Expr << (Ident ^ id));
          },

        // Match.
        In(Expr) * (T(MatchExpr) << End) * (!T(Brace) * (!T(Brace))++)[Expr] *
            T(Brace)[Brace] >>
          [](Match& _) {
            return MatchExpr << (Expr << _[Expr]) << (ExprSeq << *_(Brace));
          },

        // Lambda.
        In(Expr) * ParamsPat[Params] *
            ~(T(Colon) * (!T(SymbolId, "->"))++[Type]) * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << (Params << *_[Params]) << make_type(_[Type])
                          << lambda_body(_(Brace), _[Rhs]);
          },

        // Lambda with a single parameter.
        In(Expr) * T(Ident)[Ident] * T(SymbolId, "->") *
            (T(Brace)[Brace] / Any++[Rhs]) >>
          [](Match& _) {
            return Lambda << (Params
                              << (ParamDef << _(Ident) << make_type() << Body))
                          << make_type() << lambda_body(_(Brace), _[Rhs]);
          },

        // Lambda without parameters.
        In(Expr) * T(Brace)[Brace] >>
          [](Match& _) {
            return Lambda << Params << make_type() << lambda_body(_(Brace), {});
          },

        // Dot.
        In(Expr) * (T(Dot) << End) * T(Ident, SymbolId)[Ident] *
            ~TypeArgsPat[TypeArgs] >>
          [](Match& _) {
            return Dot << _(Ident) << (TypeArgs << *_[TypeArgs]);
          },

        // Ref.
        In(Expr) * T(Ident, "ref") >> [](Match&) -> Node { return Ref; },

        // Function name.
        In(Expr) *
            (T(Ident, SymbolId) * ~TypeArgsPat *
             (T(DoubleColon) * T(Ident, SymbolId) *
              ~TypeArgsPat)++)[FuncName] >>
          [](Match& _) {
            Node name = FuncName;
            Node elem;

            for (auto& n : _[FuncName])
            {
              if (n->in({Ident, SymbolId}))
                elem = NameElement << n << TypeArgs;
              else if (n == Bracket)
                (elem / TypeArgs) << *n;
              else if (n == DoubleColon)
                name << elem;
            }

            name << elem;
            return name;
          },

        // Builtin or FFI.
        In(Expr) * T(TripleColon) * T(FuncName)[FuncName] >>
          [](Match& _) { return TripleColon << *_(FuncName); },

        // Array literal: ::(expr, ...)
        In(Expr) * T(DoubleColon) * T(Paren)[Paren] >>
          [](Match& _) { return ArrayLit << (Expr << _(Paren)); },

        // If.
        In(Expr) * (T(If) << End) * (!T(Brace))++[Expr] * T(Brace)[Brace] >>
          [](Match& _) {
            return If << (Expr << _[Expr])
                      << (Block << lambda_body(_(Brace), {}));
          },

        // Else.
        In(Expr) * ElseLhsPat[Lhs] * (T(Else) << End) *
            (!T(Equals, Else) * (!T(Equals, Else))++)[Rhs] >>
          [](Match& _) -> Node {
          return Else << (Expr << _[Lhs])
                      << (Block << lambda_body(_(Rhs), _[Rhs]));
        },

        // While.
        In(Expr) * (T(While) << End) * (!T(Brace) * (!T(Brace))++)[While] *
            T(Brace)[Brace] >>
          [](Match& _) {
            return While << (Expr << _[While])
                         << (Block << lambda_body(_(Brace), {}));
          },

        // When.
        In(Expr) * (T(When) << End) * (!T(Lambda))++[Expr] *
            T(Lambda)[Lambda] >>
          [](Match& _) {
            auto lambda = _(Lambda);
            return When << (Expr << _[Expr]) << clone(lambda / Type)
                        << (Expr << lambda);
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

        // Remove semicolon marker groups.
        T(Group) << (T(Semi) * End) >> [](Match&) -> Node { return {}; },

        // Groups are expressions.
        In(Body, Expr, ExprSeq, Tuple) * T(Group)[Group] >>
          [](Match& _) { return Expr << *_[Group]; },

        // Parens are expression sequences.
        In(Body, Expr, Tuple) * T(Paren)[Paren] >>
          [](Match& _) { return ExprSeq << *_[Paren]; },

        // Lists are tuples.
        In(Body, ExprSeq, Tuple) * T(List)[List] >>
          [](Match& _) -> Node { return Expr << (Tuple << *_[List]); },

        In(Expr) * T(List)[List] >>
          [](Match& _) -> Node { return Tuple << *_[List]; },

        // An ExprSeq containing an Expr that contains a Tuple is a Tuple.
        In(Expr) * T(ExprSeq) << ((T(Expr) << (T(Tuple)[Tuple] * End)) * End) >>
          [](Match& _) { return _(Tuple); },

        // An empty ExprSeq is an empty Tuple.
        In(Expr) * T(ExprSeq) << End >> [](Match&) -> Node { return Tuple; },

        // Flatten ArrayLit: absorb Tuple children (2+ elements).
        T(ArrayLit) << ((T(Expr) << (T(Tuple)[Tuple] * End)) * End) >>
          [](Match& _) { return ArrayLit << *_[Tuple]; },

        // Flatten ArrayLit: absorb ExprSeq children (0-1 elements).
        T(ArrayLit) << ((T(Expr) << (T(ExprSeq)[ExprSeq] * End)) * End) >>
          [](Match& _) { return ArrayLit << *_[ExprSeq]; },

        // Remove empty expressions.
        T(Expr) << End >> [](Match&) -> Node { return {}; },
      }};

    p.post([&](auto top) -> size_t {
      Nodes deps;

      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node->get_contains_error())
        {
          // Do nothing.
        }
        else if (node == Group)
        {
          node->parent()->replace(node, err(node, "Syntax error"));
          ok = false;
        }
        else if ((node == Use) && (node->front() != TypeName))
        {
          // This is a package dependency.
          ok = false;
          size_t i = 0;
          auto id = node->at(i++);
          Node url;

          if (id == Ident)
            url = node->at(i++);
          else
            url = std::move(id);

          auto tag = node->size() > i ? node->at(i++) : Node();
          auto dir = node->size() > i ? node->at(i) : Node();

          // Clone or update the dependency.
          Dependency dep(url, tag, dir);

          if (!dep.fetch())
            return false;

          // Parse the dependency.
          auto p_ast = parse.sub_parse(dep.src_path);

          // If there's no AST, there were no source files.
          if (!p_ast)
          {
            node << err(url, "Dependency has no source files");
            return false;
          }

          if (p_ast->get_contains_error())
          {
            top << p_ast;
            node << err(url, "Failed to parse dependency");
            return false;
          }

          // Insert the dependency's AST.
          assert(p_ast == Directory);
          deps.push_back((Directory ^ dep.hash) << *p_ast);

          // Rewrite the Use.
          auto tn = TypeName << (NameElement << (Ident ^ dep.hash) << TypeArgs);

          if (id && (node->parent() != ClassBody))
            id =
              err(node, "Dependency aliases can only be declared in classes");
          else if (id)
            id = TypeAlias << id << TypeParams << Where << (Type << tn);
          else
            id = Use << tn;

          node->parent()->replace(node, id);
        }
        else if (node == TypeAlias)
        {
          if (node->parent() != ClassBody)
          {
            node->parent()->replace(
              node, err(node, "Type aliases can only be declared in classes"));
            ok = false;
          }
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
        else if (node == New)
        {
          if (node->size() != 1)
          {
            node << err(node, "Expected field initializers");
            ok = false;
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

          for (auto& child : *node)
          {
            if (!child->in(wfTypeElement))
            {
              node->replace(child, err(child, "Expected a type"));
              ok = false;
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
            if (child != TypeParam)
            {
              node->replace(child, err(child, "Expected a type parameter"));
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
          auto parent = node->parent();

          for (auto& child : *node)
          {
            if ((child == Expr) && (parent != Lambda))
            {
              node->replace(
                child, err(child, "Case values can only be used in lambdas"));
              ok = false;
            }
            else if (!child->in({ParamDef, Expr}))
            {
              node->replace(child, err(child, "Expected a parameter"));
              ok = false;
            }
          }

          // Function parameters must have explicit type annotations.
          // Lambda parameters may omit types (inferred from call site).
          if (parent == Function)
          {
            for (auto& child : *node)
            {
              if ((child == ParamDef) && ((child / Type)->front() == TypeVar))
              {
                node->replace(
                  child,
                  err(
                    child / Ident,
                    "Function parameters must have a type annotation"));
                ok = false;
              }
            }
          }
        }
        else if (node == NewArgs)
        {
          for (auto& child : *node)
          {
            if (child != NewArg)
            {
              node->replace(child, err(child, "Expected a field initializer"));
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
        else if (node == MatchExpr)
        {
          auto seq = node / ExprSeq;

          if (seq->empty())
          {
            seq << err(node, "Match expression must have at least one case");
            ok = false;
          }

          for (auto& child : *seq)
          {
            if ((child->size() != 1) || (child->front() != Lambda))
            {
              seq->replace(child, err(child, "Expected a match case lambda"));
              ok = false;
            }
          }
        }
        else if (
          (node == ClassDef) && ((node / Shape) != Shape) &&
          !node->get_contains_error())
        {
          // Auto-create: generate a default `create` function for non-shape
          // classes that don't already have one. The create takes parameters
          // matching the class fields and calls `new` with them.
          auto cls_body = node / ClassBody;
          bool has_create = false;

          for (auto& child : *cls_body)
          {
            if (
              (child == Function) && ((child / Lhs) == Rhs) &&
              (child / Ident)->location().view() == "create")
            {
              has_create = true;
              break;
            }
          }

          if (!has_create)
          {
            Node params = Params;
            Node new_args = NewArgs;

            for (auto& child : *cls_body)
            {
              if (child != FieldDef)
                continue;

              auto field_ident = child / Ident;
              auto field_type = child / Type;

              params
                << (ParamDef << clone(field_ident) << clone(field_type)
                             << Body);
              new_args
                << (NewArg << clone(field_ident)
                           << (Expr
                               << (FuncName
                                   << (NameElement << clone(field_ident)
                                                   << TypeArgs))));
            }

            auto type = make_selftype(node / Ident);

            cls_body
              << (Function << Rhs << (Ident ^ "create") << TypeParams << params
                           << type << Where
                           << (Body << (Expr << (New << new_args))));
          }
        }
        else if (node == ClassBody)
        {
          // Check for conflicting functions: same name, handedness, and
          // parameter count.
          struct FuncSig
          {
            Token hand;
            std::string name;
            size_t arity;
          };

          std::vector<std::pair<FuncSig, Node>> seen;

          for (auto& child : *node)
          {
            if (child != Function)
              continue;

            FuncSig sig{
              (child / Lhs)->type(),
              std::string((child / Ident)->location().view()),
              (child / Params)->size()};

            bool conflict = false;

            for (auto& [prev_sig, prev_node] : seen)
            {
              if (
                (prev_sig.hand == sig.hand) && (prev_sig.name == sig.name) &&
                (prev_sig.arity == sig.arity))
              {
                node->replace(
                  child,
                  err(child, "Conflicting function definition")
                    << errmsg("Previous definition was here:")
                    << errloc(prev_node));
                conflict = true;
                ok = false;
                break;
              }
            }

            if (!conflict)
              seen.push_back({sig, child});
          }
        }

        return ok;
      });

      if (!deps.empty())
      {
        for (auto& d : deps)
          top << d;

        auto [ast, count, changes] = p.run(top);
        return changes;
      }

      return 0;
    });

    return p;
  }
}
