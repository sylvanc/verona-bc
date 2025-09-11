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
        In(Expr) * T(Lambda)[Lambda]
            << (T(Params)[Params] * T(Type)[Type] * T(Body)[Body]) >>
          [](Match& _) {
            auto lambda = _(Lambda);
            std::set<Location> freevars;

            lambda->traverse([&](auto node) {
              bool ok = true;

              if (node == Error)
              {
                ok = false;
              }
              else if (node == Ident)
              {
                auto defs = node->lookup();

                if (std::any_of(defs.begin(), defs.end(), [&](auto& d) {
                      return d->in({ParamDef, Let, Var}) &&
                        node->lookup(lambda).empty();
                    }))
                  freevars.emplace(node->location());
              }

              return ok;
            });

            auto id = _.fresh(l_lambda);

            // TODO: find "free" type parameters.
            // Populate TypeParams for the lambda class.
            Node typeparams = TypeParams;

            // For each free type parameter, create a type argument.
            Node typeargs = TypeArgs;

            for (auto& tp : *typeparams)
            {
              typeargs
                << (TypeName
                    << (TypeElement << (Ident ^ tp / Ident) << TypeArgs));
            }

            auto type = Type
              << (TypeName << (TypeElement << (Ident ^ id) << typeargs));

            Node classbody = ClassBody;
            Node create_params = Params;
            Node create_args = Args;
            Node apply_params = Params
              << (ParamDef << (Ident ^ "self") << clone(type) << Body)
              << *_(Params);
            Node apply_body = Body;
            Node new_args = Tuple;

            for (auto& freevar : freevars)
            {
              classbody << (FieldDef << (Ident ^ freevar) << Type << Body);
              create_params << (ParamDef << (Ident ^ freevar) << Type << Body);
              create_args << (Expr << (Ident ^ freevar));

              apply_body
                << (Expr
                    << (Equals
                        << (Expr << (Let << (Ident ^ freevar) << Type))
                        << (Expr
                            << (Load
                                << (Expr
                                    << (FieldRef << (Expr << (Ident ^ "self"))
                                                 << (FieldId ^ freevar)))))));

              new_args << (Expr << (Ident ^ freevar));
            }

            apply_body << *_(Body);

            return Seq
              << (Lift
                  << ClassBody
                  << (ClassDef
                      << (Ident ^ id) << typeparams << Where
                      << (classbody
                          << (Function
                              << Rhs << (Ident ^ "create") << TypeParams
                              << create_params << type << Where
                              << (Body
                                  << (Expr << New
                                           << (ExprSeq << (Expr << new_args)))))
                          << (Function << Rhs << (Ident ^ "apply") << TypeParams
                                       << apply_params << _(Type) << Where
                                       << apply_body))))
              << (Call << (QName
                           << (QElement << (Ident ^ id) << clone(typeargs)))
                       << create_args);
          },

        // Default arguments.
        In(ClassBody) * T(Function)[Function]
            << (T(Lhs, Rhs)[Lhs] * T(Ident, SymbolId)[Ident] *
                T(TypeParams)[TypeParams] * T(Params)[Params] * T(Type)[Type] *
                T(Where)[Where] * T(Body)[Body]) >>
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

          return Seq << (Function << clone(_(Lhs)) << clone(ident)
                                  << clone(_(TypeParams)) << params_0
                                  << clone(_(Type)) << clone(_(Where))
                                  << (Body
                                      << (Expr << call_func(
                                            ident, _(TypeParams), args))))
                     << _(Function);
        },

        // Auto-RHS.
        In(ClassBody) * T(Function)[Function]
            << (T(Lhs) * T(Ident, SymbolId)[Ident] * T(TypeParams)[TypeParams] *
                T(Params)[Params] * T(Type)[Type] * T(Where)[Where]) >>
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
                     << clone(_(Params)) << type << clone(_(Where))
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
        else if (node->get_contains_error())
        {
          // Do nothing.
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
