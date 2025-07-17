#include "../lang.h"

namespace vc
{
  // Sythetic locations.
  inline const auto l_lambda = Location("lambda");

  Node call_func(Node ident, Node tps, Node args)
  {
    auto cls = ident->parent(ClassDef);
    auto cls_tps = cls / TypeParams;
    return Call << (QName << (QElement << clone(cls / Ident)
                                       << make_typeargs(cls_tps))
                          << (QElement << clone(ident) << make_typeargs(tps)))
                << args;
  }

  PassDef sugar()
  {
    PassDef p{
      "sugar",
      wfPassSugar,
      dir::topdown,
      {
        // Turn lambdas into anonymous classes.
        In(Expr) * T(Lambda)
            << (T(TypeParams)[TypeParams] * T(Params)[Params] * T(Type)[Type] *
                T(Body)[Body]) >>
          [](Match& _) {
            // TODO: free variables
            // - field for each one
            // - create arg for each one
            // - new arg for each one
            // - unpack each one in apply
            // - create call arg for each one

            auto id = _.fresh(l_lambda);
            auto type = Type
              << (TypeName << (TypeElement << (Ident ^ id) << TypeArgs));
            auto params = _(Params);
            params->push_front(
              ParamDef << (Ident ^ "self") << clone(type) << Body);

            return Seq << (Lift
                           << ClassBody
                           << (ClassDef
                               << (Ident ^ id) << TypeParams
                               << (ClassBody
                                   << (Function << Rhs << (Ident ^ "create")
                                                << TypeParams << Params << type
                                                << (Body << (Expr << New)))
                                   << (Function << Rhs << (Ident ^ "apply")
                                                << _(TypeParams) << params
                                                << _(Type) << _(Body)))))
                       << (Call
                           << (QName
                               << (QElement << (Ident ^ id) << TypeArgs)
                               << (QElement << (Ident ^ "create") << TypeArgs))
                           << Args);
          },

        // Default arguments.
        In(ClassBody) * T(Function)[Function]
            << (T(Lhs, Rhs)[Lhs] * T(Ident, SymbolId)[Ident] *
                T(TypeParams)[TypeParams] * T(Params)[Params] * T(Type)[Type] *
                T(Body)[Body]) >>
          [](Match& _) -> Node {
          auto params = _(Params);
          if (params->empty())
            return NoChange;

          auto last_param = params->back();
          auto body = last_param / Body;

          if (body->empty())
            return NoChange;

          (last_param / Body) = Body;
          auto ident = _(Ident);
          auto params_0 = clone(params);
          params_0->pop_back();
          Node args = Args;

          for (auto& param : *params_0)
            args << (Expr << clone(param / Ident));

          args << (Expr << (ExprSeq << *body));

          return Seq << (Function
                         << clone(_(Lhs)) << clone(ident)
                         << clone(_(TypeParams)) << params_0 << clone(_(Type))
                         << (Body
                             << (Expr
                                 << call_func(ident, _(TypeParams), args))))
                     << _(Function);
        },

        // Auto-RHS.
        In(ClassBody) * T(Function)[Function]
            << (T(Lhs) * T(Ident, SymbolId)[Ident] * T(TypeParams)[TypeParams] *
                T(Params)[Params] * T(Type)[Type]) >>
          [](Match& _) -> Node {
          // Check if an RHS function with the same name and arity exists.
          auto ident = _(Ident);
          auto arity = _(Params)->size();
          auto cls = _(Function)->parent(ClassBody);

          for (auto& def : *cls)
          {
            if (
              (def == Function) && ((def / Lhs) == Rhs) &&
              (def / Ident)->equals(ident) && ((def / Params)->size() == arity))
              return NoChange;
          }

          // If Type is a RefType, unwrap it, otherwise empty.
          auto type = _(Type);

          if (!type->empty() && (type->front() == RefType))
            type = Type << clone(type->front()->front());
          else
            type = Type;

          // Forward the arguments.
          Node args = Args;

          for (auto& param : *_(Params))
            args << (Expr << clone(param / Ident));

          // Create the RHS function.
          auto rhs =
            Function << Rhs << clone(ident) << clone(_(TypeParams))
                     << clone(_(Params)) << type
                     << (Body
                         << (Expr
                             << (Load
                                 << (Expr << Ref
                                          << call_func(
                                               ident, _(TypeParams), args)))));

          return Seq << _(Function) << rhs;
        },
      }};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node == ParamDef)
        {
          auto body = node / Body;

          if (body->empty())
          {
            node->replace(body);
          }
          else
          {
            node->replace(
              body, err(body, "Default arguments must be at the end"));
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
