#include "../lang.h"

namespace vc
{
  namespace
  {
    // Synthetic location for generated functype shape names.
    inline const auto l_functype = Location("fn");

    // Given a FuncType node and the Top node, collect all TypeName children
    // that resolve to TypeParam definitions. Returns a list of
    // (TypeParam def node, TypeName node) pairs, deduplicated by def identity.
    struct FreeTP
    {
      Node def;  // The TypeParam definition node.
      Node name; // The FQ TypeName referencing it.
    };

    std::vector<FreeTP> collect_free_typeparams(Node ft, Node top)
    {
      std::vector<FreeTP> result;
      std::set<Node> seen;

      ft->traverse([&](auto node) {
        if (node == TypeName)
        {
          auto def = find_def(top, node);

          if (def && (def == TypeParam) && seen.insert(def).second)
            result.push_back({def, node});
        }

        return true;
      });

      return result;
    }

    // Extract the parameter types from a FuncType's Lhs.
    // NoArgType -> empty list.
    // TupleType -> list of children.
    // Single wfType -> list of one.
    Nodes functype_params(Node ft)
    {
      auto lhs = ft / Lhs;

      if (lhs == NoArgType)
        return {};

      if (lhs == TupleType)
      {
        Nodes result;

        for (auto& child : *lhs)
          result.push_back(child);

        return result;
      }

      return {lhs};
    }
  }

  PassDef functype()
  {
    PassDef p{
      "functype",
      wfPassFuncType,
      dir::bottomup,
      {
        T(FuncType)[FuncType] >>
        [](Match& _) -> Node {
          auto ft = _(FuncType);
          auto top = ft->parent({Top});
          auto enclosing_cls = ft->parent(ClassDef);
          assert(top);
          assert(enclosing_cls);

          auto cls_path = scope_path(enclosing_cls);
          auto cls_ta = fq_typeargs(cls_path, enclosing_cls / TypeParams);

          // Collect free type params referenced inside the FuncType.
          auto free_tps = collect_free_typeparams(ft, top);

          // Fresh name for the synthetic shape.
          auto id = _.fresh(l_functype);

          // Build TypeParams for the shape (one per free TP).
          Node typeparams = TypeParams;

          for (auto& ftp : free_tps)
          {
            auto tp_name =
              std::string((ftp.def / Ident)->location().view());
            typeparams << (TypeParam << (Ident ^ tp_name) << (Type << TypeVar));
          }

          // Build a substitution from original FQ TypeParam references
          // to the shape's own FQ TypeParam references.
          // old: enclosing_scope::tp_name -> new: cls_path::shape_id::tp_name
          std::map<std::string, Node> tp_subst;

          for (size_t i = 0; i < free_tps.size(); i++)
          {
            // Key: serialized original TypeName.
            std::string key;

            for (auto& elem : *(free_tps[i].name))
              key += std::string((elem / Ident)->location().view()) + "::";

            // Value: the shape's own TypeParam reference.
            auto tp_name =
              std::string((free_tps[i].def / Ident)->location().view());
            Node new_tn = TypeName;

            for (auto& s : cls_path)
              new_tn << (NameElement << clone(s / Ident) << TypeArgs);

            new_tn << (NameElement << (Ident ^ id) << TypeArgs);
            new_tn << (NameElement << (Ident ^ tp_name) << TypeArgs);
            tp_subst[key] = new_tn;
          }

          // Substitute TypeParam references in a type subtree (clone + rewrite).
          // Collect matching nodes first, then mutate after traversal.
          auto subst_type = [&](Node type_node) -> Node {
            auto result = clone(type_node);
            Nodes to_replace;

            result->traverse([&](auto node) {
              if (node == TypeName)
              {
                std::string key;

                for (auto& elem : *node)
                  key += std::string((elem / Ident)->location().view()) + "::";

                if (tp_subst.find(key) != tp_subst.end())
                  to_replace.push_back(node);
              }

              return true;
            });

            for (auto& node : to_replace)
            {
              std::string key;

              for (auto& elem : *node)
                key += std::string((elem / Ident)->location().view()) + "::";

              node->parent()->replace(node, clone(tp_subst.at(key)));
            }

            return result;
          };

          // Build the apply method params: (self, ...functype_args)
          auto param_types = functype_params(ft);
          Node apply_params = Params;
          apply_params
            << (ParamDef << (Ident ^ "self") << (Type << TypeSelf));

          for (size_t i = 0; i < param_types.size(); i++)
          {
            auto param_name = "a" + std::to_string(i);
            apply_params
              << (ParamDef << (Ident ^ param_name)
                           << subst_type(Type << clone(param_types[i])));
          }

          // Build the apply method return type.
          auto ret_type = subst_type(Type << clone(ft / Rhs));

          // Shape function bodies need a placeholder: _builtin::none::create.
          // The ident pass has already run, so we use FQ names.
          Node shape_body =
            Body
            << (Expr
                << (FuncName
                    << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                    << (NameElement << (Ident ^ "none") << TypeArgs)
                    << (NameElement << (Ident ^ "create") << TypeArgs)));

          // Create the shape ClassDef.
          Node shape_def =
            ClassDef << Shape << (Ident ^ id) << typeparams << Where
                     << (ClassBody
                         << (Function << Rhs << (Ident ^ "apply") << TypeParams
                                     << apply_params << ret_type << Where
                                     << shape_body));

          // Build TypeArgs for the replacement TypeName (external use):
          // FQ refs to the original type params (not the shape's own).
          Node outer_ta = TypeArgs;

          for (auto& ftp : free_tps)
            outer_ta << (Type << clone(ftp.name));

          // Build FQ TypeName for the replacement type.
          Node fq_tn_outer = TypeName;

          for (auto& s : cls_path)
          {
            if (s == enclosing_cls)
              fq_tn_outer
                << (NameElement << clone(enclosing_cls / Ident)
                                << clone(cls_ta));
            else
              fq_tn_outer << (NameElement << clone(s / Ident) << TypeArgs);
          }

          fq_tn_outer << (NameElement << (Ident ^ id) << clone(outer_ta));

          return Seq << (Lift << ClassBody << shape_def) << fq_tn_outer;
        },
    }};

    return p;
  }
}
