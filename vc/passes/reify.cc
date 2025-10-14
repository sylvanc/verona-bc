#include "../lang.h"
#include "../reifications.h"

namespace vc
{
  Node classid_to_primitive(Node classid)
  {
    auto name = classid->location().view();

    if (name.starts_with("builtin."))
    {
      if (name.ends_with("any[0]"))
        return Dyn;
      else if (name.ends_with("none[0]"))
        return None;
      else if (name.ends_with("bool[0]"))
        return Bool;
      else if (name.ends_with("i8[0]"))
        return I8;
      else if (name.ends_with("i16[0]"))
        return I16;
      else if (name.ends_with("i32[0]"))
        return I32;
      else if (name.ends_with("i64[0]"))
        return I64;
      else if (name.ends_with("u8[0]"))
        return U8;
      else if (name.ends_with("u16[0]"))
        return U16;
      else if (name.ends_with("u32[0]"))
        return U32;
      else if (name.ends_with("u64[0]"))
        return U64;
      else if (name.ends_with("ilong[0]"))
        return ILong;
      else if (name.ends_with("ulong[0]"))
        return ULong;
      else if (name.ends_with("isize[0]"))
        return ISize;
      else if (name.ends_with("usize[0]"))
        return USize;
      else if (name.ends_with("f32[0]"))
        return F32;
      else if (name.ends_with("f64[0]"))
        return F64;
    }

    return NoChange;
  }

  PassDef reify()
  {
    auto rs = std::make_shared<Reifications>();

    PassDef p{
      "reify",
      wfIR,
      dir::bottomup,
      {
        // Turn ClassId into a primitive type, if needed.
        T(ClassId)[ClassId] >>
          [](Match& _) { return classid_to_primitive(_(ClassId)); },

        // Turn FuncType into Dyn.
        T(FuncType) >> [](Match&) -> Node { return Dyn; },

        // Turn TupleType into Array Dyn.
        T(TupleType)[TupleType] >> [](Match&) { return Array << Dyn; },

        // Turn TypeNameReified into a ClassId or primitive type.
        T(TypeNameReified)[TypeNameReified] >> [=](Match& _) -> Node {
          auto tn = _(TypeNameReified);
          auto& r = rs->get_reification(tn);

          // If it's an unreified type, remove it.
          if (!r.instance)
            return {};

          assert(r.instance == ClassDef);
          auto id = r.instance / Ident;

          // If this has already been turned into a primitive, use it. If the
          // target is a primitive type, this may not have happened yet.
          if (id != ClassId)
            return clone(id);

          // Check for a complex primitive: Array, Ref, or Cown.
          if (r.def->parent(ClassDef) == rs->builtin)
          {
            auto name = (r.def / Ident)->location().view();

            if (name == "array")
              return Array << clone(r.subst.begin()->second);
            else if (name == "ref")
              return Ref << clone(r.subst.begin()->second);
            else if (name == "cown")
              return Cown << clone(r.subst.begin()->second);
          }

          // Check for a primitive, and either return that or the ClassId.
          auto prim = classid_to_primitive(id);

          if (prim != NoChange)
            return prim;

          return clone(id);
        },

        // An intersection type is Dyn.
        T(Isect) >> [](Match&) -> Node { return Dyn; },

        // A union type that contains Dyn is Dyn.
        T(Union)[Union] >> [](Match& _) -> Node {
          if (_(Union)->contains(Dyn))
            return Dyn;

          return NoChange;
        },

        // An empty type is Dyn.
        T(Type)[Type] >> [](Match& _) -> Node {
          auto t = _(Type);

          if (t->empty())
            return Dyn;

          return t->front();
        },

        // Turn a ParamDef into a Param.
        T(ParamDef) << (T(Ident)[Ident] * Any[Type]) >>
          [](Match& _) { return Param << (LocalId ^ _(Ident)) << _(Type); },

        // Lift library definitions.
        In(ClassBody) * T(Lib)[Lib] >>
          [](Match& _) { return Lift << Top << _(Lib); },

        // Lift reified functions.
        T(Function)[Function] << (T(Lhs, Rhs) * T(FunctionId)) >>
          [](Match& _) {
            auto f = _(Function);
            return Lift << Top
                        << (Func << (f / Ident) << (f / Params) << (f / Type)
                                 << Vars << (f / Labels));
          },

        // Lift reified classes.
        T(ClassDef)[ClassDef] << (T(Shape, None) * !T(Ident)) >>
          [](Match& _) -> Node {
          auto c = _(ClassDef);
          bool primitive = (c / Ident) != ClassId;

          Node f;
          Node m = Methods;
          auto cls = (primitive ? Primitive : Class) << (c / Ident);

          if (!primitive)
          {
            // Primitives don't have fields.
            f = Fields;
            cls << f;
          }
          else if ((c / Ident) == Dyn)
          {
            // Don't emit an implementation for `any`.
            return {};
          }

          cls << m;

          for (auto& n : *(c / ClassBody))
          {
            if (n == FieldDef)
            {
              if (primitive)
              {
                n->parent() << err(n, "Primitive types can't have fields.");
                return NoChange;
              }

              f << (Field << (FieldId ^ (n / Ident)) << (n / Type));
            }
            else if (n == Method)
            {
              m << n;
            }
          }

          return Lift << Top << cls;
        },

        // Strip handedness from function calls.
        T(Call)
            << (T(LocalId)[Lhs] * T(Lhs, Rhs) * T(FunctionId)[FunctionId] *
                T(Args)[Args]) >>
          [](Match& _) -> Node {
          return Call << _(Lhs) << _(FunctionId) << _(Args);
        },

        // Use a MethodId in Lookup.
        T(Lookup)
            << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Lhs, Rhs) *
                T(Ident, SymbolId) * T(TypeArgs) * T(Int) *
                T(MethodId)[MethodId]) >>
          [](Match& _) { return Lookup << _(Lhs) << _(Rhs) << _(MethodId); },

        // Pass [T] instead of T to NewArray and NewArrayConst.
        T(NewArray)
            << (T(LocalId)[Lhs] * (!T(Array))[Type] * T(LocalId)[Rhs]) >>
          [](Match& _) {
            return NewArray << _(Lhs) << (Array << _(Type)) << _(Rhs);
          },

        T(NewArrayConst)
            << (T(LocalId)[Lhs] * (!T(Array))[Type] * T(Int)[Rhs]) >>
          [](Match& _) {
            return NewArrayConst << _(Lhs) << (Array << _(Type)) << _(Rhs);
          },

        // Use WhenDyn instead of When, and pass cown T instead of T.
        T(When)
            << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Args)[Args] *
                Any[Type]) >>
          [](Match& _) {
            return WhenDyn << _(Lhs) << _(Rhs) << _(Args) << (Cown << _(Type));
          },

        // Elide unused copies.
        T(Copy) << (T(LocalId)[Lhs] * T(LocalId)) >> [](Match& _) -> Node {
          auto lhs = _(Lhs);
          auto f = lhs->parent(Labels);
          auto found = false;

          f->traverse([&](Node& node) {
            if (
              (node == LocalId) && (node != lhs) &&
              (node->location() == lhs->location()))
            {
              found = true;
              return false;
            }

            return true;
          });

          if (!found)
            return {};

          return NoChange;
        },
      }};

    p.pre([=](auto top) {
      top->traverse([&](auto node) {
        if (node == Symbol)
        {
          rs->schedule(node, {}, true);
          return false;
        }

        return true;
      });

      rs->start(top);
      rs->run();
      return 0;
    });

    // Delete remaining ClassDef. Do this late, otherwise lifting doesn't work.
    p.post([=](auto top) {
      Nodes to_remove;

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
        else if (node == Var)
        {
          auto f = node->parent(Func);

          if (f)
            (f / Vars) << (LocalId ^ (node / Ident));

          to_remove.push_back(node);
          ok = false;
        }
        else if (node->in({Use, TypeAlias, ClassDef, Function}))
        {
          to_remove.push_back(node);
          ok = false;
        }

        return ok;
      });

      for (auto& node : to_remove)
        node->parent()->replace(node);

      return 0;
    });

    return p;
  }
}
