#include "../lang.h"

namespace vc
{
  // Sythetic locations.
  inline const auto l_lambda = Location("lambda");

  Node call_func(Node ident, Node tps, Node args)
  {
    auto cls = ident->parent(ClassDef);
    auto func = ident->parent(Function);
    auto cls_path = scope_path(cls);
    auto cls_ta = fq_typeargs(cls_path, cls / TypeParams);
    auto func_ta = fq_typeargs(scope_path(func), tps);

    Node fn = FuncName;

    for (auto& s : cls_path)
    {
      if (s == cls)
        fn << (NameElement << clone(cls / Ident) << cls_ta);
      else
        fn << (NameElement << clone(s / Ident) << TypeArgs);
    }

    fn << (NameElement << clone(ident) << func_ta);
    return Call << fn << args;
  }

  PassDef sugar()
  {
    PassDef p{
      "sugar",
      wfPassSugar,
      dir::topdown,
      {
        // Rewrite `when` arguments.
        In(Expr) * T(When)
            << ((T(Expr)[Args] << (T(Tuple)[Tuple] * End)) * T(Type)[Type] *
                (T(Expr)[Expr] << T(Lambda)[Lambda])) >>
          [](Match& _) {
            if (_(Tuple)->size() != (_(Lambda) / Params)->size())
            {
              return err(_(Tuple), "When argument count must match lambda")
                << errmsg("Lambda is here:") << errloc(_(Lambda));
            }

            return When << (Args << *_(Tuple)) << _(Type) << _(Expr);
          },

        In(Expr) * T(When)
            << (T(Expr)[Expr] * T(Type)[Type] *
                (T(Expr)[Expr] << T(Lambda)[Lambda])) >>
          [](Match& _) {
            if ((_(Lambda) / Params)->size() != 1)
            {
              return err(_(Expr), "When argument count must match lambda")
                << errmsg("Lambda is here:") << errloc(_(Lambda));
            }

            return When << (Args << _(Expr)) << _(Type) << _(Expr);
          },

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
              else if (node == LocalId)
              {
                if (node->lookup(lambda).empty())
                  freevars.emplace(node->location());
              }

              return ok;
            });

            auto id = _.fresh(l_lambda);

            // Build FQ scope prefix for the enclosing class. The Lift
            // will place the lambda ClassDef into this class's ClassBody,
            // so all FQ references to the lambda class need this prefix.
            auto enclosing_cls = lambda->parent(ClassDef);
            assert(enclosing_cls);
            auto cls_path = scope_path(enclosing_cls);
            auto cls_ta = fq_typeargs(cls_path, enclosing_cls / TypeParams);

            // Find free type parameters from enclosing scopes.
            // A free type param is one defined in a Function or ClassDef
            // that encloses the lambda. After ident, references to it are
            // fully qualified TypeNames like scope1::scope2::T.
            struct FreeTP
            {
              std::string name;
              Nodes path; // scope_path of the defining scope
            };

            std::vector<FreeTP> free_tps;
            std::set<std::string> seen_tp_names;

            {
              auto scope = lambda->parent({ClassDef, Function});

              while (scope)
              {
                auto sp = scope_path(scope);

                for (auto& tp : *(scope / TypeParams))
                {
                  auto name = std::string((tp / Ident)->location().view());

                  if (seen_tp_names.insert(name).second)
                    free_tps.push_back({name, sp});
                }

                scope = scope->parent({ClassDef, Function});
              }
            }

            // Check if a TypeName is a FQ reference to a free type param.
            // Match: NameElement idents == scope_path idents + tp name.
            auto is_free_tp = [&](Node tn, size_t& idx) -> bool {
              for (size_t i = 0; i < free_tps.size(); i++)
              {
                auto& ftp = free_tps[i];

                if (tn->size() != ftp.path.size() + 1)
                  continue;

                bool match = true;

                for (size_t j = 0; j < ftp.path.size(); j++)
                {
                  if (
                    (tn->at(j) / Ident)->location().view() !=
                    (ftp.path[j] / Ident)->location().view())
                  {
                    match = false;
                    break;
                  }
                }

                if (
                  match &&
                  ((tn->at(ftp.path.size()) / Ident)->location().view() ==
                   ftp.name))
                {
                  idx = i;
                  return true;
                }
              }

              return false;
            };

            // Collect all TypeName references to free type params.
            std::vector<std::pair<Node, size_t>> tp_refs;

            lambda->traverse([&](auto node) {
              if (node == TypeName)
              {
                size_t idx;

                if (is_free_tp(node, idx))
                  tp_refs.push_back({node, idx});
              }

              return true;
            });

            // Rewrite TypeName references inside the lambda from
            // enclosing-scope type params to the lambda class's own.
            // New path: cls_path :: lambda_id :: tp_name.
            for (auto& [old_tn, idx] : tp_refs)
            {
              Node new_tn = TypeName;

              for (auto& s : cls_path)
                new_tn << (NameElement << clone(s / Ident) << TypeArgs);

              new_tn << (NameElement << (Ident ^ id) << TypeArgs);
              new_tn
                << (NameElement << (Ident ^ free_tps[idx].name) << TypeArgs);
              old_tn->parent()->replace(old_tn, new_tn);
            }

            // Populate TypeParams for the lambda class.
            Node typeparams = TypeParams;

            for (auto& ftp : free_tps)
              typeparams << (TypeParam << (Ident ^ ftp.name) << make_type(_));

            // Build TypeArgs for internal use (self type): FQ refs to
            // the lambda class's own type params.
            Node lambda_ta = TypeArgs;

            for (auto& ftp : free_tps)
            {
              Node tp_tn = TypeName;

              for (auto& s : cls_path)
                tp_tn << (NameElement << clone(s / Ident) << TypeArgs);

              tp_tn << (NameElement << (Ident ^ id) << TypeArgs);
              tp_tn << (NameElement << (Ident ^ ftp.name) << TypeArgs);
              lambda_ta << (Type << tp_tn);
            }

            // Build TypeArgs for creation site: FQ refs to the
            // enclosing scope's type params (the originals).
            Node outer_ta = TypeArgs;

            for (auto& ftp : free_tps)
            {
              Node tp_tn = TypeName;

              for (auto& s : ftp.path)
                tp_tn << (NameElement << clone(s / Ident) << TypeArgs);

              tp_tn << (NameElement << (Ident ^ ftp.name) << TypeArgs);
              outer_ta << (Type << tp_tn);
            }

            // Build FQ TypeName for use inside the lambda class
            // (self type, create return type).
            Node fq_tn = TypeName;

            for (auto& s : cls_path)
            {
              if (s == enclosing_cls)
                fq_tn
                  << (NameElement << clone(enclosing_cls / Ident)
                                  << clone(cls_ta));
              else
                fq_tn << (NameElement << clone(s / Ident) << TypeArgs);
            }

            fq_tn << (NameElement << (Ident ^ id) << clone(lambda_ta));

            auto type = Type << clone(fq_tn);

            // Build FQ TypeName for the creation site call.
            Node fq_tn_create = TypeName;

            for (auto& s : cls_path)
            {
              if (s == enclosing_cls)
                fq_tn_create
                  << (NameElement << clone(enclosing_cls / Ident)
                                  << clone(cls_ta));
              else
                fq_tn_create << (NameElement << clone(s / Ident) << TypeArgs);
            }

            fq_tn_create << (NameElement << (Ident ^ id) << clone(outer_ta));

            Node classbody = ClassBody;
            Node create_params = Params;
            Node create_args = Args;
            Node apply_params = Params
              << (ParamDef << (Ident ^ "self") << clone(type) << Body)
              << *_(Params);
            Node apply_body = Body;
            Node new_args = NewArgs;

            for (auto& freevar : freevars)
            {
              auto typevar = make_type(_);
              classbody << (FieldDef << (Ident ^ freevar) << typevar);
              create_params
                << (ParamDef << (Ident ^ freevar) << clone(typevar) << Body);
              create_args << (Expr << (LocalId ^ freevar));

              apply_body
                << (Expr
                    << (Equals
                        << (Expr
                            << (Let << (Ident ^ freevar) << clone(typevar)))
                        << (Expr
                            << (Load
                                << (Expr
                                    << (FieldRef << (Expr << (LocalId ^ "self"))
                                                 << (FieldId ^ freevar)))))));

              new_args
                << (NewArg << (Ident ^ freevar)
                           << (Expr << (LocalId ^ freevar)));
            }

            apply_body << *_(Body);

            return Seq
              << (Lift << ClassBody
                       << (ClassDef
                           << None << (Ident ^ id) << typeparams << Where
                           << (classbody
                               << (Function
                                   << Rhs << (Ident ^ "create") << TypeParams
                                   << create_params << type << Where
                                   << (Body << (Expr << (New << new_args))))
                               << (Function << Rhs << (Ident ^ "apply")
                                            << TypeParams << apply_params
                                            << _(Type) << Where
                                            << apply_body))))
              << (Call << (FuncName
                           << *clone(fq_tn_create)
                           << (NameElement << (Ident ^ "create") << TypeArgs))
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

          // Generate a function with one fewer parameters.
          auto ident = _(Ident);
          auto params_0 = clone(params);
          params_0->pop_back();

          // Remove the default value from all parameters.
          for (auto& param : *params)
            (param / Body) = Body;

          Node args = Args;

          // Call the original function with the default final argument.
          for (auto& param : *params_0)
            args << (Expr << (LocalId ^ (param / Ident)));

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

          // Forward the arguments.
          Node args = Args;

          for (auto& param : *_(Params))
            args << (Expr << (LocalId ^ (param / Ident)));

          // Create the RHS function.
          auto rhs =
            Function << Rhs << clone(ident) << clone(_(TypeParams))
                     << clone(_(Params)) << make_type(_) << clone(_(Where))
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
