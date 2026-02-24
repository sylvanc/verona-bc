#include "../lang.h"

namespace vc
{
  PassDef application()
  {
    PassDef p{
      "application",
      wfPassApplication,
      dir::topdown,
      {
        // Ref.
        In(Expr) * (T(Ref) << End) * ValuePat[Expr] >>
          [](Match& _) { return Ref << (Expr << _(Expr)); },

        // Hash.
        In(Expr) * (T(Hash) << End) * ValuePat[Expr] >>
          [](Match& _) { return Hash << (Expr << _(Expr)); },

        // Infix function with RHS tuple.
        In(Expr) * ValuePat[Lhs] * T(FuncName)[FuncName] * T(Tuple)[Tuple] >>
          [](Match& _) {
            return Call << _(FuncName)
                        << (Args << (Expr << _(Lhs)) << *_(Tuple));
          },

        // Infix function with RHS value.
        In(Expr) * ValuePat[Lhs] * T(FuncName)[FuncName] * ValuePat[Rhs] >>
          [](Match& _) {
            return Call << _(FuncName)
                        << (Args << (Expr << _(Lhs)) << (Expr << _(Rhs)));
          },

        // Prefix function with RHS tuple.
        In(Expr) * T(FuncName)[FuncName] * T(Tuple)[Tuple] >>
          [](Match& _) { return Call << _(FuncName) << (Args << *_(Tuple)); },

        // Prefix function with RHS value.
        In(Expr) * T(FuncName)[FuncName] * ValuePat[Rhs] >>
          [](Match& _) {
            return Call << _(FuncName) << (Args << (Expr << _(Rhs)));
          },

        // Zero-argument function.
        In(Expr) * T(FuncName)[FuncName] * End >>
          [](Match& _) { return Call << _(FuncName) << Args; },

        // Infix method with RHS tuple.
        In(Expr) * ValuePat[Lhs] * T(MethodName)[MethodName] *
            T(Tuple)[Tuple] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs) << (Args << *_(Tuple));
          },

        // Infix method with RHS value.
        In(Expr) * ValuePat[Lhs] * T(MethodName)[MethodName] * ValuePat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs)
                           << (Args << (Expr << _(Rhs)));
          },

        // Prefix method with RHS tuple.
        In(Expr) * T(MethodName)[MethodName] *
            (T(Tuple) << (T(Expr)[Lhs] * (T(Expr)++)[Rhs])) >>
          [](Match& _) {
            return CallDyn << (_(Lhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs) << (Args << _[Rhs]);
          },

        // Prefix method with RHS value.
        In(Expr) * T(MethodName)[MethodName] * ValuePat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Rhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs) << Args;
          },

      }};

    p.post([](auto top) {
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
        else if (node == Expr)
        {
          if (node->size() != 1)
          {
            node->parent()->replace(node, err(node, "Expected an expression"));
            ok = false;
          }
        }
        else if (node->in({Ref, Hash}))
        {
          if (node->empty())
          {
            node->parent()->replace(node, err(node, "Expected an expression"));
            ok = false;
          }
        }
        else if (node == MethodName)
        {
          node->parent()->replace(
            node, err(node, "Expected at least one argument to this method"));
          ok = false;
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
