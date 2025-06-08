#include "../lang.h"

namespace vc
{
  // Equals
  // Tuple,
  // Ref,
  // Try,
  // Lambda,
  // QName,
  // Method, done
  // Call, done
  // CallDyn, done
  // If, done
  // While, done
  // For,
  // When,
  // Else, done

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
          [](Match& _) {
            // TODO: experimenting
            return Equals << (Lhs << *_[Lhs]) << _(Rhs);
          },

        In(Lhs)++ * T(Expr)[Expr] >>
          [](Match& _) {
            // TODO: experimenting
            return Lhs << *_[Expr];
          },

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

        // A Method in a CallDyn is a Lookup.
        In(CallDyn) *
            (T(Method)
             << (T(LocalId)[LocalId] * T(Ident, SymbolId)[Ident] *
                 T(TypeArgs)[TypeArgs])) >>
          [](Match& _) {
            // TODO: what to do with the TypeArgs?
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Lookup << (LocalId ^ id) << _(LocalId)
                                           << (MethodId ^ _(Ident))))
                       << (LocalId ^ id) << (LocalId ^ _(LocalId));
          },

        // Any other Method is a CallDyn.
        --In(CallDyn) *
            (T(Method)
             << (T(LocalId)[LocalId] * T(Ident, SymbolId)[Ident] *
                 T(TypeArgs)[TypeArgs])) >>
          [](Match& _) {
            // TODO: what to do with the TypeArgs?
            auto lookup = _.fresh(l_local);
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (Lookup << (LocalId ^ lookup) << _(LocalId)
                                           << (MethodId ^ _(Ident))))
                       << (Lift
                           << Body
                           << (CallDyn << (LocalId ^ id) << (LocalId ^ lookup)
                                       << (Args << (LocalId ^ _(LocalId)))))
                       << (LocalId ^ id);
          },

        // CallDyn.
        In(Expr) *
            (T(CallDyn)
             << (T(LocalId)[LocalId] * T(LocalId)[Arg] * T(Args)[Args])) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift << Body
                                << (CallDyn << (LocalId ^ id) << _(LocalId)
                                            << (Args << _(Arg) << *_[Args])))
                       << (LocalId ^ id);
          },

        // Call.
        In(Expr) * (T(Call) << (T(QName)[QName] * T(Args)[Args])) >>
          [](Match& _) {
            auto id = _.fresh(l_local);
            return Seq << (Lift
                           << Body
                           << (Call << (LocalId ^ id) << _(QName) << _(Args)))
                       << (LocalId ^ id);
          },

        // Treat all Args as ArgCopy at this stage.
        In(Args) * T(LocalId)[LocalId] >>
          [](Match& _) { return Arg << ArgCopy << _(LocalId); },

        // Compact RHS LocalId.
        T(Expr) << (T(LocalId)[LocalId] * End) >>
          [](Match& _) { return _(LocalId); },

        // Combine non-terminal LocalId with an incomplete copy.
        // This is for `if` and `else`.
        In(Body) * T(LocalId)[Rhs] * (T(Copy) << (T(LocalId)[Lhs] * End)) >>
          [](Match& _) { return Copy << _(Lhs) << _(Rhs); },

        // Discard non-terminal LocalId.
        In(Body) * T(LocalId) * ++Any >> [](Match&) -> Node { return {}; },

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
