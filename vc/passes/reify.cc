#include "../lang.h"
#include "../subtype.h"

namespace vc
{
  const auto string_type = TypeName
    << (NameElement << (Ident ^ "_builtin") << TypeArgs)
    << (NameElement << (Ident ^ "string") << TypeArgs);

  const std::map<std::string_view, Node> primitive_types = {
    {"none", None},
    {"bool", Bool},
    {"i8", I8},
    {"i16", I16},
    {"i32", I32},
    {"i64", I64},
    {"u8", U8},
    {"u16", U16},
    {"u32", U32},
    {"u64", U64},
    {"ilong", ILong},
    {"ulong", ULong},
    {"isize", ISize},
    {"usize", USize},
    {"f32", F32},
    {"f64", F64},
    {"ptr", Ptr},
  };

  struct Reifier
  {
    Reifier() {}

    void run(Node& top_)
    {
      top = top_;
      builtin = top->look(Location("_builtin")).front();

      // Create a call to main and reify it.
      auto main_module = top->front();
      assert(main_module == ClassDef);
      assert((main_module / TypeParams)->empty());

      auto id = top->fresh();
      auto main_call = Call
        << (LocalId ^ id) << Rhs
        << (FuncName << (NameElement << (Ident ^ "main") << TypeArgs)) << Args;
      main_call = reify_call(main_call, {}, main_module);

      // Iteratively reify any classes, type aliases, or functions that we
      // scheduled for reification.
      while (!worklist.empty())
      {
        auto r = worklist.back();
        worklist.pop_back();

        if (r->def == ClassDef)
          reify_class(*r);
        else if (r->def == TypeAlias)
          reify_typealias(*r);
        else if (r->def == Function)
          reify_function(*r);
        else
          assert(false);
      }

      // Add an entry point for main.
      top
        << (Func << (FunctionId ^ "@main") << Params << I32 << Vars
                 << (Labels
                     << (Label << (LabelId ^ "start") << (Body << main_call)
                               << (Return << (LocalId ^ id)))));
    }

  private:
    // Each ClassDef (including primitives), TypeAlias, or Function that we
    // reify gets a Reification struct.
    struct Reification
    {
      Node def;
      NodeMap<Node> subst;
      Node id;
      Node reification;
    };

    Node top;
    Node builtin;
    NodeMap<std::vector<Reification>> map;
    std::vector<Reification*> worklist;

    void reify_class(Reification& r)
    {
      // Treat a shape as a dynamic type. We could build a set of all concrete
      // types that implement the shape.
      if ((r.def / Shape) == Shape)
      {
        r.id = Dyn;
        return;
      }

      // Check if this is a primitive type.
      if (r.def->parent(ClassDef) == builtin)
      {
        auto find = primitive_types.find((r.def / Ident)->location().view());

        if (find != primitive_types.end())
        {
          r.id = find->second;
          r.reification = Primitive << r.id << Methods;
          return;
        }
      }

      // Iterate through the fields in the class.
      Node fields = Fields;

      for (auto& f : *(r.def / ClassBody))
      {
        if (f != FieldDef)
          continue;

        fields
          << (Field << (FieldId ^ (f / Ident))
                    << reify_type(f / Type, r.subst, r.def));
      }

      // Store the reified class.
      r.reification = Class << r.id << fields << Methods;
    }

    void reify_typealias(Reification& r)
    {
      // Store the reified type alias.
      r.reification = TypeAlias << r.id
                                << reify_type(r.def / Type, r.subst, r.def);
    }

    bool reify_function(Reification& r)
    {
      // Reify the function signature.
      auto r_type = reify_type(r.def / Type, r.subst, r.def);
      Node params = Params;

      for (auto& p : *(r.def / Params))
      {
        params
          << (Param << (LocalId ^ (p / Ident))
                    << reify_type(p / Type, r.subst, r.def));
      }

      // Reify the function body.
      Node vars = Vars;
      Node labels = clone(r.def / Labels);

      for (auto& l : *labels)
      {
        Node body = l / Body;

        // No work required: Copy, Move, math ops on existing values.

        // TODO:
        // RegisterRef | FieldRef | ArrayRef | ArrayRefConst | NewArray |
        // NewArrayConst | Load | Store | Lookup | CallDyn |
        // When | FFI

        body->traverse([&](Node& n) {
          if (n == body)
            return true;

          if (n->in({Const, Convert}))
          {
            reify_primitive(n / Type);
          }
          else if (n == ConstStr)
          {
            reify_typename(string_type, {}, top);
          }
          else if (n == Typetest)
          {
            n / Type = reify_type(n / Type, r.subst, r.def);
            reify_primitive(Bool);
          }
          else if (n->in({Eq, Ne, Lt, Le, Gt, Ge}))
          {
            reify_primitive(Bool);
          }
          else if (n->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
          {
            reify_primitive(F64);
          }
          else if (n == Bits)
          {
            reify_primitive(U64);
          }
          else if (n == Len)
          {
            reify_primitive(USize);
          }
          else if (n == MakePtr)
          {
            reify_primitive(Ptr);
          }
          else if (n == Var)
          {
            vars << (Var << (LocalId ^ (n / Ident)));
          }
          else if (n == New)
          {
            // TODO: need to get the reified type of this.
            // use it to turn field map into sequential args.
            n / Type = reify_type(n / Type, r.subst, r.def);
          }
          else if (n == Call)
          {
            n->parent()->replace(n, reify_call(n, r.subst, r.def));
          }

          return false;
        });
      }

      r.reification = Func << r.id << params << r_type << vars << labels;
      return true;
    }

    // Turn a type into an IR type. The IR doesn't have intersection types,
    // structural types, or tuple types.
    Node
    reify_type(const Node& type, const NodeMap<Node>& subst, const Node& scope)
    {
      if (type == Type)
        return reify_type(type->front(), subst, scope);

      // Use Dyn until we turn function types into shapes.
      if (type == FuncType)
        return Dyn;

      // Use [Dyn] for now.
      if (type == TupleType)
        return Array << Dyn;

      if (type == Union)
      {
        Node r = Union;

        for (auto& t : *type)
        {
          auto rt = reify_type(t, subst, scope);

          // A union that contains a dynamic type is just dynamic. A union that
          // contains a union is flattened.
          if (rt == Dyn)
            return Dyn;
          else if (rt == Union)
            r << *rt;
          else
            r << rt;
        }

        return r;
      }

      if (type == Isect)
      {
        Node r = Dyn;

        for (auto& t : *type)
        {
          auto rt = reify_type(t, subst, scope);

          // Encapsulate rt in a union.
          if (rt != Union)
            rt = Union << rt;

          if (r == Dyn)
          {
            // A dynamic result means all types, so the intersection is rt.
            r = rt;
          }
          else
          {
            // Intersect the existing union with this one.
            Node nr = Union;

            for (auto& existing : *r)
            {
              // Keep this existing type if it also exists in rt. Dynamic types
              // in the intersection are ignored.
              bool found = std::any_of(rt->begin(), rt->end(), [&](auto& c) {
                return (c != Dyn) && existing->equals(c);
              });

              // Keep only unique types.
              if (found && std::none_of(nr->begin(), nr->end(), [&](auto& u) {
                    return u->equals(existing);
                  }))
              {
                nr << existing;
              }
            }

            r = nr;
          }
        }

        return r;
      }

      if (type == TypeName)
        return reify_typename(type, subst, scope);

      assert(false);
      return {};
    }

    // Get the reification and return the ClassId or TypeId.
    Node reify_typename(
      const Node& tn, const NodeMap<Node>& subst, const Node& scope)
    {
      return get_reification(tn, subst, scope, [](auto& def) {
        return def->in({ClassDef, TypeAlias});
      });
    }

    // Reify a primitive from a wfPrimitiveType.
    void reify_primitive(const Node& type)
    {
      for (auto& [k, v] : primitive_types)
      {
        // Look for this primitive type.
        if (type != v)
          continue;

        // Look up the class definition by name.
        auto defs = builtin->look(Location(std::string(k)));
        assert(defs.size() == 1);
        auto def = defs.front();
        assert(def == ClassDef);
        assert((def / TypeParams)->size() == 0);

        auto& r_vec = map[def];

        // If this primitive type has already been reified, we're done.
        if (!r_vec.empty())
          return;

        // Store this reification.
        r_vec.push_back({def, {}, type, Primitive << type << Methods});
        return;
      }
    }

    Node
    reify_call(const Node& call, const NodeMap<Node>& subst, const Node& scope)
    {
      auto hand = (call / Lhs)->type();
      auto arity = (call / Args)->size();

      auto funcid =
        get_reification(call / FuncName, subst, scope, [&](auto& def) {
          return (def == Function) && ((def / Params)->size() == arity) &&
            ((def / Lhs) == hand);
        });

      return Call << (call / LocalId) << funcid << (call / Args);
    }

    // Given a TypeName or FuncName, a substitution map, and a scope, find or
    // create a reification and return the ClassId, TypeId, or FunctionId. The
    // accept function is used to filter definitions, such as looking for a
    // function with a specific arity and handedness.
    template<typename F>
    Node get_reification(
      const Node& name, const NodeMap<Node>& subst, const Node& scope, F accept)
    {
      assert(name->in({TypeName, FuncName}));
      auto def = scope;

      for (auto& elem : *name)
      {
        if (elem == TypeParent)
        {
          if (def == Top)
            return err(name, "No parent scope for type name");

          def = def->parent({Top, ClassDef, TypeAlias, Function});
        }
        else
        {
          assert(elem == NameElement);
          auto defs = def->look((elem / Ident)->location());
          bool found = false;

          for (auto& d : defs)
          {
            // TODO: don't call accept at every level, only at the end
            // in between, always look for ClassDef or TypeParam
            if (accept(d))
            {
              found = true;
              def = d;
              break;
            }
          }

          if (!found)
          {
            auto e = err(elem, "No definition for name element");

            if (def == Top)
              return e << errmsg("Resolving at the top level.");
            else
              return e << errmsg("Resolving here:") << errloc(def / Ident);
          }

          if (def == TypeParam)
          {
            auto find = subst.find(def);

            if (find == subst.end())
              return err(elem, "No substitution for type parameter");

            // TODO: find->second is the type argument
            // it can be an algebraic type
            assert(find->second == Type);

            if (find->second->front() == TypeName)
            {
              // TODO: need to find the def and subst
            }
          }
        }
      }

      // TODO: proper name expansion with substitution.
      // TODO: good error messages.
      for (auto& n : *name)
      {
        auto defs = def->look((n / Ident)->location());

        if (defs.empty())
          return {false, nullptr};

        if (n == name->back())
        {
          for (auto& d : defs)
          {
            if (accept(d))
            {
              def = d;
              break;
            }
          }
        }
        else
        {
          def = defs.front();

          if (def != ClassDef)
            return {false, nullptr};
        }

        size_t count = (def / TypeParams)->size();
        assert((n / TypeArgs)->size() == count);

        for (size_t i = 0; i < count; i++)
          subst[(def / TypeParams)->at(i)] = (n / TypeArgs)->at(i);
      }

      // TODO: from here on is already good

      auto& r_vec = map[def];

      for (auto& existing : r_vec)
      {
        assert(existing.def == def);
        assert(
          std::equal(
            existing.subst.begin(),
            existing.subst.end(),
            subst.begin(),
            subst.end(),
            [](auto& lhs, auto& rhs) { return lhs.first == rhs.first; }));

        if (std::equal(
              existing.subst.begin(),
              existing.subst.end(),
              subst.begin(),
              subst.end(),
              [&](auto& lhs, auto& rhs) {
                return Subtype(lhs.second, rhs.second) &&
                  Subtype(rhs.second, lhs.second);
              }))
        {
          // This reification already exists.
          return clone(existing.id);
        }
      }

      // Store this reification.
      r_vec.push_back({def, subst, make_id(def, r_vec.size()), {}});

      // Schedule this for reification.
      auto& r = r_vec.back();
      worklist.push_back(&r);
      return clone(r.id);
    }

    Node make_id(const Node& def, size_t index)
    {
      // Identifiers take the form `a::b::c::3`.
      assert(def->in({ClassDef, TypeAlias, Function}));
      auto id = std::string((def / Ident)->location().view());
      auto parent = def->parent({Top, ClassDef, TypeAlias, Function});

      while (parent)
      {
        id = std::format("{}::{}", (parent / Ident)->location().view(), id);
        parent = parent->parent({Top, ClassDef, TypeAlias, Function});
      }

      if (def == Function)
      {
        // A function adds arity and handedness.
        id = std::format(
          "{}.{}{}",
          id,
          (def / Params)->size(),
          (def / Lhs) == Lhs ? ".ref" : "");
      }

      id = std::format("{}::{}", id, index);

      if (def == ClassDef)
        return ClassId ^ id;
      else if (def == TypeAlias)
        return TypeId ^ id;
      else if (def == Function)
        return FunctionId ^ id;

      assert(false);
      return {};
    }
  };

  PassDef reify()
  {
    auto rs = std::make_shared<Reifications>();

    PassDef p{
      "reify",
      wfIR,
      dir::bottomup,
      {
        // // Lift library definitions.
        // In(ClassBody) * T(Lib)[Lib] >>
        //   [](Match& _) { return Lift << Top << _(Lib); },

        // // Use a MethodId in Lookup.
        // T(Lookup)
        //     << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Lhs, Rhs) *
        //         T(Ident, SymbolId) * T(TypeArgs) * T(Int) *
        //         T(MethodId)[MethodId]) >>
        //   [](Match& _) { return Lookup << _(Lhs) << _(Rhs) << _(MethodId); },

        // // Pass [T] instead of T to NewArray and NewArrayConst.
        // T(NewArray)
        //     << (T(LocalId)[Lhs] * (!T(Array))[Type] * T(LocalId)[Rhs]) >>
        //   [](Match& _) {
        //     return NewArray << _(Lhs) << (Array << _(Type)) << _(Rhs);
        //   },

        // T(NewArrayConst)
        //     << (T(LocalId)[Lhs] * (!T(Array))[Type] * T(Int)[Rhs]) >>
        //   [](Match& _) {
        //     return NewArrayConst << _(Lhs) << (Array << _(Type)) << _(Rhs);
        //   },

        // // Use WhenDyn instead of When, and pass cown T instead of T.
        // T(When)
        //     << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Args)[Args] *
        //         Any[Type]) >>
        //   [](Match& _) {
        //     return WhenDyn << _(Lhs) << _(Rhs) << _(Args) << (Cown <<
        //     _(Type));
        //   },

        // // Elide unused copies.
        // T(Copy) << (T(LocalId)[Lhs] * T(LocalId)) >> [](Match& _) -> Node {
        //   auto lhs = _(Lhs);
        //   auto f = lhs->parent(Labels);
        //   auto found = false;

        //   f->traverse([&](Node& node) {
        //     if (
        //       (node == LocalId) && (node != lhs) &&
        //       (node->location() == lhs->location()))
        //     {
        //       found = true;
        //       return false;
        //     }

        //     return true;
        //   });

        //   if (!found)
        //     return {};

        //   return NoChange;
        // },
      }};

    p.pre([=](auto top) {
      Reifier().run(top);
      return 0;
    });

    return p;
  }
}
