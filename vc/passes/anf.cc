#include "../lang.h"

namespace vc
{
  // Sythetic locations.
  inline const auto l_local = Location("local");
  inline const auto l_label = Location("label");

  Node type_nomatch()
  {
    return TypeName << (TypeElement << (Ident ^ "std") << TypeArgs)
                    << (TypeElement << (Ident ^ "builtin") << TypeArgs)
                    << (TypeElement << (Ident ^ "nomatch") << TypeArgs);
  }

  Node make_nomatch(Node localid)
  {
    assert(localid == LocalId);
    return Stack << (LocalId ^ localid) << type_nomatch() << Args;
  }

  Node test_nomatch(Node dst, Node src)
  {
    assert(dst == LocalId);
    assert(src == LocalId);
    return Typetest << (LocalId ^ dst) << (LocalId ^ src) << type_nomatch();
  }

  std::string flat_name(Node ident, Node typeargs, size_t argc, bool ref)
  {
    // TODO: typeargs?
    (void)typeargs;
    return std::format(
      "{}.{}{}", ident->location().view(), argc, ref ? ".ref" : "");
  }

  std::string flat_qname(Node qname, size_t argc, bool ref)
  {
    std::stringstream ss;
    bool first = true;

    for (auto& child : *qname)
    {
      if (first)
        first = false;
      else
        ss << ".";

      // TODO: typeargs?
      ss << (child / Ident)->location().view();
    }

    ss << "." << argc;

    if (ref)
      ss << ".ref";

    return ss.str();
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

    // TODO: check that a version with this arity exists.
    auto argc = _(Args) ? _(Args)->size() : 0;
    auto name = flat_qname(_(QName), argc, ref);
    auto id = _.fresh(l_local);
    auto res = lvalue ? (Ref << (LocalId ^ id)) : (LocalId ^ id);
    return Seq << (Lift << Body
                        << (Call << (LocalId ^ id) << (FunctionId ^ name)
                                 << (Args << *_[Args])))
               << res;
  }

  Node make_calldyn(Match& _, bool lvalue, bool ref)
  {
    // lvalue is true if the call appears on the LHS of an assignment.
    // ref is true if lvalue is true, or if the call is in a Ref node.
    auto argc = _(Args) ? _(Args)->size() : 0;
    auto name = flat_name(_(Ident), _(TypeArgs), argc + 1, ref);
    auto fn = _.fresh(l_local);
    auto id = _.fresh(l_local);
    auto res = lvalue ? (Ref << (LocalId ^ id)) : (LocalId ^ id);
    return Seq << (Lift << Body
                        << (Lookup << (LocalId ^ fn) << (LocalId ^ _(LocalId))
                                   << (MethodId ^ name)))
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
            auto id = _(Body)->parent(Function) / Ident;
            return Labels << (Label << (LabelId ^ id) << _(Body));
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
                           << (Move << (LocalId ^ id) << (LocalId ^ _(Lhs))))
                       << (Lift << Body << (Copy << _(Lhs) << _(Rhs)))
                       << (LocalId ^ id);
          },

        // Store.
        T(Equals) << ((T(Ref) << T(LocalId)[Lhs]) * T(LocalId)[Rhs]) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift
                           << Body
                           << (Store << (LocalId ^ id) << _(Lhs) << _(Rhs)))
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
                           << (ArrayRefConst << (LocalId ^ ref)
                                             << (LocalId ^ _(Rhs))
                                             << (Int ^ std::to_string(idx++))))
                  << (Lift << Body
                           << (Load << (LocalId ^ val) << (LocalId ^ ref)));
              tuple << (Equals << l << (LocalId ^ val));
            }

            return seq << tuple;
          },

        // Invalid l-values.
        In(Lhs) * T(Lambda, QName, If, Else, While, For, When)[Lhs] >>
          [](Match& _) { return err(_(Lhs), "can't assign to this"); },

        // If expression.
        In(Expr) * T(If)[If] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body << (_(If) << (LocalId ^ id)))
                       << (LocalId ^ id);
          },

        // If body.
        // TODO: distinguish typetest by param count
        In(Body) * T(If)
            << (T(Expr)[Cond] *
                (T(Lambda)
                 << ((T(TypeParams) << End) * (T(Params) << End) *
                     T(Type)[Type] * T(Body)[Body])) *
                T(LocalId)[LocalId]) >>
          [](Match& _) {
            // TODO: what do we do with the Type?
            auto on_true = _.fresh(l_label);
            auto join = _.fresh(l_label);
            return Seq << make_nomatch(_(LocalId))
                       << (Cond << _(Cond) << (LabelId ^ on_true)
                                << (LabelId ^ join))
                       << (Label << (LabelId ^ on_true)
                                 << (_(Body) << (Copy << (LocalId ^ _(LocalId)))
                                             << (Jump << (LabelId ^ join))))
                       << (Label << (LabelId ^ join) << Body);
          },

        // While expression.
        In(Expr) * T(While)[While] >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body << (_(While) << (LocalId ^ id)))
                       << (LocalId ^ id);
          },

        // While body.
        In(Body) * T(While)
            << (T(Expr)[Cond] *
                (T(Lambda)
                 << ((T(TypeParams) << End) * (T(Params) << End) *
                     T(Type)[Type] * T(Body)[Body])) *
                T(LocalId)[LocalId]) >>
          [](Match& _) {
            // TODO: what do we do with the Type?
            auto cond = _.fresh(l_label);
            auto body = _.fresh(l_label);
            auto join = _.fresh(l_label);
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
        In(Expr) * T(Else) << (T(Expr)[Lhs] * (T(Expr)[Rhs] << !T(Lambda))) >>
          [](Match& _) {
            return Else << _(Lhs)
                        << (Expr
                            << (Lambda << TypeParams << Params << Type
                                       << (Body << _(Rhs))));
          },

        // Else lambda.
        In(Expr) * T(Else)
            << (T(LocalId)[Lhs] * (T(Expr) << T(Lambda)[Lambda])) >>
          [](Match& _) {
            return Seq << (Lift << Body << (Else << _(Lhs) << _(Lambda)))
                       << (LocalId ^ _(Lhs));
          },

        // Else body.
        // TODO: distinguish typetest by param count
        In(Body) * T(Else)
            << (T(LocalId)[LocalId] *
                (T(Lambda)
                 << ((T(TypeParams) << End) * (T(Params) << End) *
                     T(Type)[Type] * T(Body)[Body]))) >>
          [](Match& _) {
            // TODO: what do we do with the Type?
            auto id = _.fresh(l_local);
            auto on_true = _.fresh(l_label);
            auto join = _.fresh(l_label);
            return Seq << test_nomatch((LocalId ^ id), _(LocalId))
                       << (Cond << (LocalId ^ id) << (LabelId ^ on_true)
                                << (LabelId ^ join))
                       << (Label << (LabelId ^ on_true)
                                 << (_(Body) << (Copy << (LocalId ^ _(LocalId)))
                                             << (Jump << (LabelId ^ join))))
                       << (Label << (LabelId ^ join) << Body);
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

        // Lift variable declarations.
        In(Expr, Lhs) * T(Let)[Let] >>
          [](Match& _) {
            return Seq << (Lift << Body << _(Let))
                       << (LocalId ^ (_(Let) / Ident));
          },

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

        In(Lhs) * CallPat >>
          [](Match& _) { return make_call(_, false, false); },

        // 0-argument static call.
        In(Expr) * (T(Ref) << (T(Expr) << T(QName)[QName])) >>
          [](Match& _) { return make_call(_, false, true); },

        In(Expr) * T(QName)[QName] >>
          [](Match& _) { return make_call(_, false, false); },

        In(Lhs) * T(QName)[QName] >>
          [](Match& _) { return make_call(_, false, false); },

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
            return Seq << (Lift << Body << (Const << (LocalId ^ id) << None))
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

        // Discard non-terminal LocalId.
        In(Body) * T(LocalId) * ++Any >> [](Match&) -> Node { return {}; },

        // Discard non-terminal Ref LocalId.
        In(Body) * (T(Ref) << T(LocalId)) * ++Any >>
          [](Match&) -> Node { return {}; },

        // A terminator followed by a Jump is an unnecessary Jump.
        In(Body) *
            T(Tailcall, TailcallDyn, Return, Raise, Throw, Cond, Jump)[Return] *
            T(Jump) * End >>
          [](Match& _) { return _(Return); },

        // A terminator that isn't at the end is an error.
        // TODO: this is matching a Lift after the terminator.
        // In(Body) *
        //     T(Tailcall, TailcallDyn, Return, Raise, Throw, Cond,
        //     Jump)[Return] * Any >>
        //   [](Match& _) { return err(_(Return), "must be a terminator"); },

        // Compact an ExprSeq with only one element.
        T(ExprSeq) << (Any[Expr] * End) >> [](Match& _) { return _(Expr); },

        // Discard leading LocalId in ExprSeq.
        In(ExprSeq) * T(LocalId) * ++Any >> [](Match&) -> Node { return {}; },
      }};

    return p;
  }
}
