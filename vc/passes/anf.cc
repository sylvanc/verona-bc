#include "../lang.h"

namespace vc
{
  // Sythetic locations.
  inline const auto l_local = Location("local");
  inline const auto l_cond = Location("cond");
  inline const auto l_body = Location("body");
  inline const auto l_join = Location("join");

  size_t field_count(Node classbody)
  {
    assert(classbody == ClassBody);
    size_t count = 0;

    for (auto& child : *classbody)
    {
      if (child == FieldDef)
        ++count;
    }

    return count;
  }

  Node type_nomatch()
  {
    return Type
      << (TypeName << (TypeElement << (Ident ^ "std") << TypeArgs)
                   << (TypeElement << (Ident ^ "builtin") << TypeArgs)
                   << (TypeElement << (Ident ^ "nomatch") << TypeArgs));
  }

  Node make_nomatch(Node localid)
  {
    assert(localid == LocalId);
    return New << (LocalId ^ localid) << type_nomatch() << Args;
  }

  Node test_nomatch(Node dst, Node src)
  {
    assert(dst == LocalId);
    assert(src == LocalId);
    return Typetest << (LocalId ^ dst) << (LocalId ^ src) << type_nomatch();
  }

  const auto CallPat = T(Call)[Call] << (T(QName)[QName] * T(Args)[Args]);

  const auto CallDynPat = T(CallDyn)[CallDyn]
    << ((T(Method)
         << (T(LocalId)[LocalId] * T(Ident, SymbolId)[Ident] *
             T(TypeArgs)[TypeArgs])) *
        T(Args)[Args]);

  const auto MethodPat = T(Method)[Method]
    << (T(LocalId)[LocalId] * T(Ident, SymbolId)[Ident] *
        T(TypeArgs)[TypeArgs]);

  Node make_call(Match& _, bool lvalue, bool ref)
  {
    // lvalue is true if the call appears on the LHS of an assignment.
    // ref is true if lvalue is true, or if the call is in a Ref node.
    auto id = _.fresh(l_local);
    auto res = lvalue ? (Ref << (LocalId ^ id)) : (LocalId ^ id);
    return Seq << (Lift << Body
                        << (Call << (LocalId ^ id) << (ref ? Lhs : Rhs)
                                 << _(QName) << (Args << *_[Args])))
               << res;
  }

  Node make_calldyn(Match& _, bool lvalue, bool ref)
  {
    // lvalue is true if the call appears on the LHS of an assignment.
    // ref is true if lvalue is true, or if the call is in a Ref node.
    auto fn = _.fresh(l_local);
    auto id = _.fresh(l_local);
    auto res = lvalue ? (Ref << (LocalId ^ id)) : (LocalId ^ id);
    auto arity = _(Args) ? _(Args)->size() + 1 : 1;
    return Seq << (Lift << Body
                        << (Lookup << (LocalId ^ fn) << (LocalId ^ _(LocalId))
                                   << (ref ? Lhs : Rhs) << _(Ident)
                                   << _(TypeArgs)
                                   << (Int ^ std::to_string(arity))))
               << (Lift << Body
                        << (CallDyn
                            << (LocalId ^ id) << (LocalId ^ fn)
                            << (Args << (LocalId ^ _(LocalId)) << *_[Args])))
               << res;
  }

  PassDef anf()
  {
    PassDef p{
      "anf",
      wfPassANF,
      dir::topdown,
      {
        // Turn an initial function body into a label.
        In(Function) * T(Body)[Body] >>
          [](Match& _) {
            return Labels << (Label << (LabelId ^ "start") << _(Body));
          },

        // New
        In(Expr) * T(New) << T(Args)[Args] >>
          [](Match& _) {
            auto args = _(Args);
            auto fields = field_count(args->parent(ClassBody));

            if (fields != args->size())
              return err(args, "New requires an argument for each field");

            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (New << (LocalId ^ id) << make_selftype(args)
                                        << args))
                       << (LocalId ^ id);
          },

        // Tuple creation.
        In(Expr) * T(Tuple)[Tuple] << (T(LocalId)++ * End) >>
          [](Match& _) {
            // Allocate a [dyn] of the right size.
            auto tuple = _(Tuple);
            auto id = _.fresh(l_local);
            auto seq = Seq
              << (Lift << Body
                       << (NewArrayConst
                           << (LocalId ^ id) << Type
                           << (Int ^ std::to_string(tuple->size()))));

            // Copy the elements into the array.
            size_t idx = 0;
            for (auto& elem : *tuple)
            {
              auto ref = _.fresh(l_local);
              auto prev = _.fresh(l_local);
              seq << (Lift << Body
                           << (ArrayRefConst
                               << (LocalId ^ ref)
                               << (Arg << ArgCopy << (LocalId ^ id))
                               << (Int ^ std::to_string(idx++))))
                  << (Lift << Body
                           << (Store << (LocalId ^ prev) << (LocalId ^ ref)
                                     << (Arg << ArgCopy << elem)));
            }

            return seq << (LocalId ^ id);
          },

        // Field reference.
        In(Expr) * T(FieldRef) << (T(LocalId)[LocalId] * T(FieldId)[FieldId]) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (FieldRef << (LocalId ^ id)
                                             << (Arg << ArgCopy << _(LocalId))
                                             << _(FieldId)))
                       << (LocalId ^ id);
          },

        // L-values.
        In(Expr) * T(Equals) << (T(Expr)[Lhs] * T(Expr)[Rhs]) >>
          [](Match& _) { return Equals << (Lhs << *_[Lhs]) << _(Rhs); },

        In(Lhs, TupleLHS) * T(Expr)[Expr] >>
          [](Match& _) { return Lhs << *_[Expr]; },

        In(Lhs) * T(Tuple)[Tuple] >>
          [](Match& _) { return TupleLHS << *_[Tuple]; },

        // Assignment.
        T(Equals) << (T(LocalId)[Lhs] * T(LocalId)[Rhs]) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift
                           << Body
                           << (Copy << (LocalId ^ id) << (LocalId ^ _(Lhs))))
                       << (Lift << Body << (Copy << _(Lhs) << _(Rhs)))
                       << (LocalId ^ id);
          },

        // Store.
        T(Equals) << ((T(Ref) << T(LocalId)[Lhs]) * T(LocalId)[Rhs]) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Store << (LocalId ^ id) << _(Lhs)
                                          << (Arg << ArgCopy << _(Rhs))))
                       << (LocalId ^ id);
          },

        // Destructuring assignment.
        T(Equals)
            << ((T(TupleLHS)[Lhs] << (T(TupleLHS, LocalId)++)) *
                T(LocalId)[Rhs]) >>
          [](Match& _) {
            // If the RHS is too short, this will throw an error.
            // If the RHS is too long, the extra values will be discarded.
            Node seq = Seq;
            Node tuple = Tuple;
            size_t idx = 0;

            for (auto& l : *_(Lhs))
            {
              auto ref = _.fresh(l_local);
              auto val = _.fresh(l_local);
              seq << (Lift << Body
                           << (ArrayRefConst
                               << (LocalId ^ ref)
                               << (Arg << ArgCopy << (LocalId ^ _(Rhs)))
                               << (Int ^ std::to_string(idx++))))
                  << (Lift << Body
                           << (Load << (LocalId ^ val) << (LocalId ^ ref)));
              tuple << (Equals << l << (LocalId ^ val));
            }

            return seq << (Expr << tuple);
          },

        // Invalid l-values.
        In(Lhs) * T(QName, If, Else, While, For, When)[Lhs] >>
          [](Match& _) { return err(_(Lhs), "Can't assign to this"); },

        // If expression.
        In(Expr) * T(If)[If] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body << (_(If) << (LocalId ^ id)))
                       << (Var << (Ident ^ id) << Type);
          },

        // If body.
        // TODO: distinguish typetest by param count
        In(Body) * T(If)
            << (T(Expr)[Cond] *
                (T(Block) << ((T(Params) << End) * T(Type) * T(Body)[Body])) *
                T(LocalId)[LocalId]) >>
          [](Match& _) {
            // TODO: what do we do with Type?
            auto body = _.fresh(l_body);
            auto join = _.fresh(l_join);
            return Seq << make_nomatch(_(LocalId))
                       << (Cond << _(Cond) << (LabelId ^ body)
                                << (LabelId ^ join))
                       << (Label << (LabelId ^ body)
                                 << (_(Body) << (Copy << (LocalId ^ _(LocalId)))
                                             << (Jump << (LabelId ^ join))))
                       << (Label << (LabelId ^ join) << Body);
          },

        // While expression.
        In(Expr) * T(While)[While] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body << (_(While) << (LocalId ^ id)))
                       << (Var << (Ident ^ id) << Type);
          },

        // While body.
        In(Body) * T(While)
            << (T(Expr)[Cond] *
                (T(Block) << ((T(Params) << End) * T(Type) * T(Body)[Body])) *
                T(LocalId)[LocalId]) >>
          [](Match& _) {
            // TODO: what do we do with Type?
            auto cond = _.fresh(l_cond);
            auto body = _.fresh(l_body);
            auto join = _.fresh(l_join);

            _(Body)->traverse([&](Node& node) {
              if (node->in({While, For, Error}))
                return false;
              else if (node == Break)
                node << (LocalId ^ _(LocalId)) << (LabelId ^ join);
              else if (node == Continue)
                node << (LocalId ^ _(LocalId)) << (LabelId ^ cond);
              return true;
            });

            return Seq << make_nomatch(_(LocalId)) << (Jump << (LabelId ^ cond))
                       << (Label << (LabelId ^ cond)
                                 << (Body
                                     << (Cond << _(Cond) << (LabelId ^ body)
                                              << (LabelId ^ join))))
                       << (Label << (LabelId ^ body)
                                 << (_(Body) << (Copy << (LocalId ^ _(LocalId)))
                                             << (Jump << (LabelId ^ cond))))
                       << (Label << (LabelId ^ join) << Body);
          },

        // Else expression.
        In(Expr) * T(Else) << (T(LocalId)[Lhs] * (T(Block)[Block])) >>
          [](Match& _) {
            return Seq << (Lift << Body << (Else << _(Lhs) << _(Block)))
                       << (LocalId ^ _(Lhs));
          },

        // Else body.
        // TODO: distinguish typetest by param count
        In(Body) * T(Else)
            << (T(LocalId)[LocalId] *
                (T(Block) << ((T(Params) << End) * T(Type) * T(Body)[Body]))) >>
          [](Match& _) {
            // TODO: what do we do with Type?
            auto id = _.fresh(l_local);
            auto body = _.fresh(l_body);
            auto join = _.fresh(l_join);
            return Seq << test_nomatch((LocalId ^ id), _(LocalId))
                       << (Cond << (LocalId ^ id) << (LabelId ^ body)
                                << (LabelId ^ join))
                       << (Label << (LabelId ^ body)
                                 << (_(Body) << (Copy << (LocalId ^ _(LocalId)))
                                             << (Jump << (LabelId ^ join))))
                       << (Label << (LabelId ^ join) << Body);
          },

        // Break, continue.
        In(Body) * T(Break, Continue)[Break]
            << (T(LocalId)[Rhs] * T(LocalId)[Lhs] * T(LabelId)[LabelId]) >>
          [](Match& _) {
            return Seq << (Copy << _(Lhs) << _(Rhs)) << (Jump << _(LabelId));
          },

        // Continuation label.
        In(Body) * (T(Label) << (T(LabelId)[LabelId] * T(Body)[Body])) *
            (!T(Label) * (!T(Label))++)[Continue] * End >>
          [](Match& _) {
            return Label << (LabelId ^ _(LabelId)) << (_(Body) << _[Continue]);
          },

        // Lift labels without reversing ordering.
        In(Labels) * T(Label)[Label] >> [](Match& _) -> Node {
          auto body = _(Label) / Body;
          Nodes labels;

          auto it = std::remove_if(body->begin(), body->end(), [&](Node& n) {
            if (n == Label)
            {
              labels.push_back(n);
              return true;
            }

            return false;
          });

          if (labels.empty())
            return NoChange;

          body->erase(it, body->end());
          auto seq = Seq << _(Label);

          for (auto& l : labels)
            seq << l;

          return seq;
        },

        // Replace Let with LocalId.
        // TODO: what about the Type?
        In(Expr, Lhs) * T(Let)[Let] >>
          [](Match& _) { return LocalId ^ (_(Let) / Ident); },

        // Lift variable declarations.
        // TODO: what about the Type?
        In(Expr, Lhs) * T(Var)[Var] >>
          [](Match& _) {
            return Seq << (Lift << Body << _(Var))
                       << (LocalId ^ (_(Var) / Ident));
          },

        // Lift literals.
        In(Expr) * T(True, False)[Bool] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Const << (LocalId ^ id) << Bool << _(Bool)))
                       << (LocalId ^ id);
          },

        In(Expr) * T(Bin, Oct, Int, Hex)[Int] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Const << (LocalId ^ id) << U64 << _(Int)))
                       << (LocalId ^ id);
          },

        In(Expr) * T(Float, HexFloat)[Float] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Const << (LocalId ^ id) << F64 << _(Float)))
                       << (LocalId ^ id);
          },

        In(Expr) * T(String, RawString)[String] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (ConstStr << (LocalId ^ id) << _(String)))
                       << (LocalId ^ id);
          },

        // Dynamic call.
        In(Expr) * T(Ref) << (T(Expr) << CallDynPat) >>
          [](Match& _) { return make_calldyn(_, false, true); },

        In(Expr) * CallDynPat >>
          [](Match& _) { return make_calldyn(_, false, false); },

        In(Lhs) * CallDynPat >>
          [](Match& _) { return make_calldyn(_, true, true); },

        // 0-argument dynamic call.
        In(Expr) * T(Ref) << (T(Expr) << MethodPat) >>
          [](Match& _) { return make_calldyn(_, false, true); },

        In(Expr) * MethodPat >>
          [](Match& _) { return make_calldyn(_, false, false); },

        In(Lhs) * MethodPat >>
          [](Match& _) { return make_calldyn(_, true, true); },

        // Static call.
        In(Expr) * (T(Ref) << (T(Expr) << CallPat)) >>
          [](Match& _) { return make_call(_, false, true); },

        In(Expr) * CallPat >>
          [](Match& _) { return make_call(_, false, false); },

        In(Lhs) * CallPat >> [](Match& _) { return make_call(_, true, true); },

        // 0-argument static call.
        In(Expr) * (T(Ref) << (T(Expr) << T(QName)[QName])) >>
          [](Match& _) { return make_call(_, false, true); },

        In(Expr) * T(QName)[QName] >>
          [](Match& _) { return make_call(_, false, false); },

        In(Lhs) * T(QName)[QName] >>
          [](Match& _) { return make_call(_, true, true); },

        // Treat all Args as ArgCopy at this stage.
        In(Args) * T(LocalId)[LocalId] >>
          [](Match& _) { return Arg << ArgCopy << _(LocalId); },

        // RHS reference creation.
        In(Expr) * T(Ref) << T(LocalId)[LocalId] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift
                           << Body
                           << (RegisterRef << (LocalId ^ id) << _(LocalId)))
                       << (LocalId ^ id);
          },

        // LHS dereference.
        In(Lhs) * T(Ref) << (T(Ref) << T(LocalId)[LocalId]) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Load << (LocalId ^ id) << _(LocalId)))
                       << (Ref << (LocalId ^ id));
          },

        // Load, from auto-RHS fields.
        In(Expr) * T(Load) << T(LocalId)[LocalId] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Load << (LocalId ^ id) << _(LocalId)))
                       << (LocalId ^ id);
          },

        // Convert.
        In(Expr, Lhs) * T(Convert)
            << (Any[Type] *
                (T(Args) << ((
                   T(Arg) << (T(ArgCopy) * T(LocalId)[LocalId]))))) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Convert << (LocalId ^ id) << _(Type)
                                            << _(LocalId)))
                       << (LocalId ^ id);
          },

        // Binop.
        In(Expr, Lhs) * T(Binop)
            << (Any[Op] *
                (T(Args)
                 << ((T(Arg) << (T(ArgCopy) * T(LocalId)[Lhs])) *
                     (T(Arg) << (T(ArgCopy) * T(LocalId)[Rhs]))))) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift
                           << Body
                           << (_(Op) << (LocalId ^ id) << _(Lhs) << _(Rhs)))
                       << (LocalId ^ id);
          },

        // Unop.
        In(Expr, Lhs) * T(Unop)
            << (Any[Op] *
                (T(Args) << (T(Arg) << (T(ArgCopy) * T(LocalId)[Lhs])))) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body << (_(Op) << (LocalId ^ id) << _(Lhs)))
                       << (LocalId ^ id);
          },

        // Nulop.
        In(Expr, Lhs) * T(Nulop) << (T(None) * T(Args)) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Const << (LocalId ^ id) << None << None))
                       << (LocalId ^ id);
          },

        In(Expr, Lhs) * T(Nulop) << ((!T(None))[Op] * T(Args)) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body << (_(Op) << (LocalId ^ id)))
                       << (LocalId ^ id);
          },

        // Compact LocalId.
        T(Expr, Lhs) << (T(LocalId)[LocalId] * End) >>
          [](Match& _) { return _(LocalId); },

        // Compact LHS Ref LocalId.
        T(Lhs) << (T(Ref) << T(LocalId)[LocalId] * End) >>
          [](Match& _) { return Ref << _(LocalId); },

        // Compact TupleLHS.
        T(Lhs) << (T(TupleLHS)[TupleLHS] * End) >>
          [](Match& _) { return _(TupleLHS); },

        // Combine non-terminal LocalId with an incomplete copy.
        // This is for `if` and `else`.
        In(Body) * T(LocalId)[Rhs] * (T(Copy) << (T(LocalId)[Lhs] * End)) >>
          [](Match& _) { return Copy << _(Lhs) << _(Rhs); },

        // If there's a terminator, elide the incomplete Copy and the Jump.
        In(Body) * (T(Jump, Return, Raise, Throw))[Return] *
            (T(Copy) << (T(LocalId) * End)) * T(Jump) * End >>
          [](Match& _) -> Node { return _(Return); },

        // Discard non-terminal LocalId.
        In(Body) * T(LocalId) * ++Any >> [](Match&) -> Node { return {}; },

        // Compact an ExprSeq with only one element.
        T(ExprSeq) << (Any[Expr] * End) >> [](Match& _) { return _(Expr); },

        // Discard leading LocalId in ExprSeq.
        In(ExprSeq) * T(LocalId) * ++Any >> [](Match&) -> Node { return {}; },

        // An empty ExprSeq is not an expression.
        T(ExprSeq)[ExprSeq] << End >>
          [](Match& _) {
            return err(_(ExprSeq), "Unexpected empty parentheses");
          },
      }};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node == Label)
        {
          // Move the terminator.
          auto body = node / Body;
          auto term = body->pop_back();

          if (term == LocalId)
            node << (Return << term);
          else if (term->in({Return, Raise, Throw, Jump, Cond}))
            node << term;
          else
          {
            // If the terminator is not a control flow node, it is an error.
            node << err(term, "Invalid terminator");
            ok = false;
          }
        }
        else if (node == Body)
        {
          for (auto& child : *node)
          {
            if (child->in({Return, Raise, Throw, Jump, Cond}))
            {
              node->replace(child, err(child, "Terminators must come last"));
              ok = false;
            }
          }
        }
        else if (node->in({Break, Continue}))
        {
          node->parent()->replace(
            node, err(node, "Break and continue must be inside a loop"));
          ok = false;
        }

        return ok;
      });

      return 0;
    });

    return p;
  }
}
