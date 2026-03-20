#include "../lang.h"

namespace vc
{
  // Synthetic location for partial application.
  inline const auto l_partial = Location("partial");

  // Check if a node is ultimately just a DontCare, looking through
  // Expr/ExprSeq wrappers (e.g., from single-arg method calls).
  static bool is_dontcare(Node node)
  {
    if (node == DontCare)
      return true;

    if (node->size() == 1 && node->in({Expr, ExprSeq}))
      return is_dontcare(node->front());

    return false;
  }

  // Build a partial application from a Call or CallDyn node.
  // Returns NoChange if no DontCare arguments are present.
  static Node partial_apply(Match& _, Node call)
  {
    auto args = call / Args;
    bool has_dontcare = false;

    for (auto& arg : *args)
    {
      if (is_dontcare(arg))
      {
        has_dontcare = true;
        break;
      }
    }

    if (!has_dontcare)
      return NoChange;

    auto id = _.fresh(l_partial);
    auto enclosing_cls = call->parent(ClassDef);
    assert(enclosing_cls);
    auto cls_path = scope_path(enclosing_cls);
    auto free_tps = collect_free_typeparams(call);

    // All types are TypeVar; infer pass resolves them from usage context.
    Node ret_type = make_type();

    // For CallDyn, capture the receiver as a field.
    std::vector<AnonClassField> fields;

    if (call->type().in({CallDyn, TryCallDyn}))
      fields.push_back({Location("$recv"), make_type(), clone(call / Expr)});

    // Split args into captured fields and apply parameters.
    Node apply_params = Params;
    Node new_call_args = Args;
    size_t cap_idx = 0;
    size_t par_idx = 0;

    for (size_t i = 0; i < args->size(); i++)
    {
      auto& arg = args->at(i);
      auto type = make_type();

      if (is_dontcare(arg))
      {
        auto pname = Location("$p" + std::to_string(par_idx++));
        apply_params << (ParamDef << (Ident ^ pname) << type << Body);
        new_call_args << (Expr << (LocalId ^ pname));
      }
      else
      {
        auto fname = Location("$cap" + std::to_string(cap_idx++));
        fields.push_back({fname, type, clone(arg)});
        new_call_args << (Expr << (LocalId ^ fname));
      }
    }

    // Build the apply body with field-loading preamble and inner call.
    Node apply_body = Body;

    for (auto& field : fields)
    {
      apply_body
        << (Expr
            << (Equals << (Expr
                           << (Let << (Ident ^ field.name)
                                   << clone(field.type)))
                       << (Expr
                           << (Load
                               << (Expr
                                   << (FieldRef << (Expr << (LocalId ^ "$self"))
                                                << (FieldId ^ field.name)))))));
    }

    if (call->type() == Call)
    {
      apply_body << (Expr << (Call << clone(call / FuncName) << new_call_args));
    }
    else if (call->type() == TryCallDyn)
    {
      apply_body
        << (Expr
            << (TryCallDyn << (Expr << (LocalId ^ "$recv"))
                           << clone(call / Ident) << clone(call / TypeArgs)
                           << new_call_args));
    }
    else
    {
      apply_body
        << (Expr
            << (CallDyn << (Expr << (LocalId ^ "$recv")) << clone(call / Ident)
                        << clone(call / TypeArgs) << new_call_args));
    }

    rewrite_typeparam_refs(apply_body, free_tps, cls_path, id);

    auto result = make_anon_class(
      id, call, free_tps, fields, apply_params, ret_type, apply_body);

    return Seq << (Lift << ClassBody << result.class_def) << result.create_call;
  }

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

        // Partial application of Call with DontCare arguments.
        In(Expr) * T(Call)[Call] >>
          [](Match& _) -> Node { return partial_apply(_, _(Call)); },

        // Partial application of CallDyn with DontCare arguments.
        In(Expr) * T(CallDyn)[CallDyn] >>
          [](Match& _) -> Node { return partial_apply(_, _(CallDyn)); },

        // Partial application of TryCallDyn with DontCare arguments.
        In(Expr) * T(TryCallDyn)[TryCallDyn] >>
          [](Match& _) -> Node { return partial_apply(_, _(TryCallDyn)); },

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
        else if (node == ParamDef && node->size() > 2)
        {
          auto last = node->back();

          if (last == Body)
            node->replace(last);
        }

        return ok;
      });

      return 0;
    });

    return p;
  }
}
