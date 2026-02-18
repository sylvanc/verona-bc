#include "../lang.h"
#include "../subtype.h"

namespace vc
{
  const auto string_type = TypeName
    << (NameElement << (Ident ^ "_builtin") << TypeArgs)
    << (NameElement << (Ident ^ "string") << TypeArgs);

  const std::map<std::string_view, Token> wrapper_types = {
    {"array", Array},
    {"cown", Cown},
    {"ref", Ref},
  };

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
        << (FuncName << (NameElement << clone(main_module / Ident) << TypeArgs)
                     << (NameElement << (Ident ^ "main") << TypeArgs))
        << Args;

      reify_call(main_call, {});

      // Iteratively reify classes/aliases/functions. Method registrations
      // happen inline: reify_class registers all existing MIs on the new
      // class, and reify_lookup registers the new MI on all existing classes.
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

      // Remove existing contents.
      top->erase(top->begin(), top->end());

      // Add an entry point for main.
      top
        << (Func << (FunctionId ^ "@main") << Params << I32 << Vars
                 << (Labels
                     << (Label << (LabelId ^ "start") << (Body << main_call)
                               << (Return << (LocalId ^ id)))));

      // Add reified classes, type aliases, and functions.
      for (auto& [_, reifications] : map)
        for (auto& r : reifications)
          top << r.reification;

      // Add reified libraries.
      for (auto& [_, lib] : libs)
        top << lib;
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

    // A MethodInvocation captures a Lookup site so we can register the
    // appropriate Method entries on every class/primitive that could receive
    // a CallDyn on this MethodId.
    struct MethodInvocation
    {
      std::string method_id; // the compiled MethodId string
      std::string name; // function name
      size_t arity; // parameter count
      Token hand; // Lhs (ref) or Rhs
      Node typeargs; // cloned TypeArgs from the Lookup
      NodeMap<Node> call_subst; // substitution context at the call site
    };

    Node top;
    Node builtin;
    NodeMap<std::deque<Reification>> map;
    std::vector<Reification*> worklist;
    std::map<Location, Node> libs;
    std::vector<MethodInvocation> method_invocations;
    std::map<std::string, std::vector<std::vector<Node>>> method_index;

    // Resolve a TypeArg through the current substitution map. If the TypeArg
    // is a Type wrapping a TypeName that resolves to a TypeParam in the subst,
    // return the substituted value. Otherwise, return the original TypeArg.
    Node resolve_typearg(const Node& arg, const NodeMap<Node>& subst)
    {
      auto inner = arg;

      // Unwrap Type node.
      if (inner == Type)
        inner = inner->front();

      if (inner != TypeName)
        return arg;

      // Navigate the FQ name to see if the last element is a TypeParam.
      Node def = top;

      for (auto& elem : *inner)
      {
        auto defs = def->look((elem / Ident)->location());

        if (defs.empty())
          return arg;

        def = defs.front();
      }

      if (def != TypeParam)
        return arg;

      // It's a TypeParam. Look it up in the subst map.
      auto find = subst.find(def);

      if (find != subst.end())
        return find->second;

      return arg;
    }

    // Check whether two substitution maps are equivalent under invariance.
    bool subst_equal(const NodeMap<Node>& a, const NodeMap<Node>& b)
    {
      return std::equal(
        a.begin(), a.end(), b.begin(), b.end(), [](auto& lhs, auto& rhs) {
          return (lhs.first == rhs.first) && Subtype(lhs.second, rhs.second) &&
            Subtype(rhs.second, lhs.second);
        });
    }

    // Compare two vectors of resolved types for invariant equality.
    bool typeargs_equal(const std::vector<Node>& a, const std::vector<Node>& b)
    {
      return std::equal(
        a.begin(), a.end(), b.begin(), b.end(), [](auto& lhs, auto& rhs) {
          return Subtype(lhs, rhs) && Subtype(rhs, lhs);
        });
    }

    // Find or create a method reification index for the given base method id
    // and type arguments resolved through the call-site substitution.
    size_t find_method_index(
      const std::string& base_id,
      const Node& typeargs,
      const NodeMap<Node>& call_subst)
    {
      std::vector<Node> resolved;

      for (auto& ta : *typeargs)
        resolved.push_back((ta == Type) ? reify_type(ta, call_subst) : Dyn);

      auto& entries = method_index[base_id];

      for (size_t i = 0; i < entries.size(); i++)
      {
        if (typeargs_equal(entries[i], resolved))
          return i;
      }

      entries.push_back(std::move(resolved));
      return entries.size() - 1;
    }

    // Find an existing reification of def with the given subst (invariant
    // subtype equivalence), or create one, schedule it, and return its id.
    //
    // For primitive and wrapper builtins, dedup uses structural id equality
    // (make_id fully resolves element types without using the index). For all
    // other defs, dedup uses substitution map equality (the index embedded in
    // generic ClassId strings would vary per call, breaking id comparison).
    Node find_or_push(const Node& def, NodeMap<Node> subst)
    {
      auto& r_vec = map[def];

      if (def->parent(ClassDef) == builtin)
      {
        auto name = (def / Ident)->location().view();

        if (
          primitive_types.find(name) != primitive_types.end() ||
          wrapper_types.find(name) != wrapper_types.end())
        {
          // Primitives and wrappers: make_id produces index-free structural
          // ids (bare tokens or Wrapper << elem_type), so id equality works.
          auto id = make_id(def, r_vec.size(), subst);

          for (auto& existing : r_vec)
          {
            if (existing.id->equals(id))
              return clone(existing.id);
          }

          r_vec.push_back({def, std::move(subst), std::move(id), {}});
          worklist.push_back(&r_vec.back());
          return clone(r_vec.back().id);
        }
      }

      // All other defs: dedup using substitution map equality.
      for (auto& existing : r_vec)
      {
        if (subst_equal(existing.subst, subst))
          return clone(existing.id);
      }

      auto id = make_id(def, r_vec.size(), subst);
      r_vec.push_back({def, std::move(subst), std::move(id), {}});
      worklist.push_back(&r_vec.back());
      return clone(r_vec.back().id);
    }

    void reify_class(Reification& r)
    {
      // Shapes are treated as dynamic types. They never reach the worklist
      // (get_reification returns Dyn early), but guard defensively.
      if ((r.def / Shape) == Shape)
      {
        r.id = Dyn;
        return;
      }

      if (r.id != ClassId)
      {
        // Primitive or wrapper type.
        r.reification = Primitive << clone(r.id) << Methods;
      }
      else
      {
        // User-defined class.
        Node fields = Fields;

        for (auto& f : *(r.def / ClassBody))
        {
          if (f != FieldDef)
            continue;

          fields
            << (Field << (FieldId ^ (f / Ident))
                      << reify_type(f / Type, r.subst));
        }

        r.reification = Class << r.id << fields << Methods;
      }

      // Register all existing method invocations on this new class.
      for (auto& mi : method_invocations)
        register_method(mi, r);
    }

    void reify_typealias(Reification& r)
    {
      // Store the reified type alias.
      r.reification = TypeAlias << r.id << reify_type(r.def / Type, r.subst);
    }

    bool reify_function(Reification& r)
    {
      // Reify the function signature.
      auto r_type = reify_type(r.def / Type, r.subst);
      Node params = Params;

      for (auto& p : *(r.def / Params))
      {
        params
          << (Param << (LocalId ^ (p / Ident))
                    << reify_type(p / Type, r.subst));
      }

      // Reify the function body.
      Node vars = Vars;
      Node labels = clone(r.def / Labels);

      // Build the set of LocalId locations that are used as sources (non-
      // destination) anywhere across all labels, including terminators.
      // A Copy whose destination is absent from this set is dead and can be
      // elided.
      std::set<Location> used_locs;

      for (auto& lbl : *labels)
      {
        // Scan each body statement, skipping the destination (first child).
        for (auto& stmt : *(lbl / Body))
        {
          for (size_t i = 1; i < stmt->size(); ++i)
          {
            stmt->at(i)->traverse([&](Node& n) {
              if (n == LocalId)
                used_locs.insert(n->location());
              return true;
            });
          }
        }

        // Scan the terminator: all LocalIds here are uses.
        (lbl / Return)->traverse([&](Node& n) {
          if (n == LocalId)
            used_locs.insert(n->location());
          return true;
        });
      }

      for (auto& l : *labels)
      {
        Node body = l / Body;
        Nodes remove;

        // No work required: Move, Load, Store, CallDyn, math ops on
        // existing values.

        // TODO:
        // RegisterRef | FieldRef | ArrayRef | ArrayRefConst

        body->traverse([&](Node& n) {
          if (n == body)
            return true;

          if (n->in({Const, Convert}))
          {
            reify_primitive(n / Type);
          }
          else if (n == ConstStr)
          {
            reify_typename(string_type, {});
          }
          else if (n == Typetest)
          {
            n / Type = reify_type(n / Type, r.subst);
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
          else if (n == Copy)
          {
            // Elide the Copy if its destination is never used as a source.
            if (used_locs.find((n / LocalId)->location()) == used_locs.end())
              remove.push_back(n);
          }
          else if (n == Var)
          {
            vars << (LocalId ^ (n / Ident));
            remove.push_back(n);
          }
          else if (n == New)
          {
            reify_new(n, r.subst);
          }
          else if (n == Lookup)
          {
            reify_lookup(n, r.subst);
          }
          else if (n == Call)
          {
            reify_call(n, r.subst);
          }
          else if (n->in({NewArray, NewArrayConst}))
          {
            n / Type = Array << reify_type(n / Type, r.subst);
          }
          else if (n == FFI)
          {
            reify_ffi(n, r);
          }
          else if (n == When)
          {
            n->parent()->replace(n, reify_when(n, r));
          }

          return false;
        });

        for (auto& n : remove)
          n->parent()->replace(n);
      }

      r.reification = Func << r.id << params << r_type << vars << labels;
      return true;
    }

    // Turn a type into an IR type. The IR doesn't have intersection types,
    // structural types, or tuple types.
    Node reify_type(const Node& type, const NodeMap<Node>& subst)
    {
      if (type == Type)
        return reify_type(type->front(), subst);

      // Use Dyn until we resolve type variables.
      if (type == TypeVar)
        return Dyn;

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
          auto rt = reify_type(t, subst);

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
          auto rt = reify_type(t, subst);

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
        return reify_typename(type, subst);

      assert(false);
      return {};
    }

    // Get the reification and return the ClassId or TypeId.
    Node reify_typename(const Node& tn, const NodeMap<Node>& subst)
    {
      return get_reification(
        tn, subst, [](auto& def) { return def->in({ClassDef, TypeAlias}); });
    }

    // Ensure a primitive type is reified. Delegates to find_or_push which
    // deduplicates and schedules via the worklist.
    void reify_primitive(const Node& type)
    {
      for (auto& [k, v] : primitive_types)
      {
        if (type != v)
          continue;

        auto defs = builtin->look(Location(std::string(k)));
        assert(defs.size() == 1);
        find_or_push(defs.front(), {});
        return;
      }
    }

    void reify_call(Node& call, const NodeMap<Node>& subst)
    {
      auto hand = (call / Lhs)->type();
      auto arity = (call / Args)->size();

      auto funcid = get_reification(call / FuncName, subst, [&](auto& def) {
        return (def == Function) && ((def / Params)->size() == arity) &&
          ((def / Lhs) == hand);
      });

      auto dst = call / LocalId;
      auto args = call / Args;
      call->erase(call->begin(), call->end());
      call << dst << funcid << args;
    }

    void reify_new(Node& n, const NodeMap<Node>& subst)
    {
      auto dst = n / LocalId;
      auto type_node = n / Type;
      auto newargs = n / NewArgs;

      // Navigate the TypeName to find the ClassDef for field ordering.
      Node def = top;
      auto tn = (type_node == Type) ? type_node->front() : type_node;

      if (tn == TypeName)
      {
        for (auto& elem : *tn)
        {
          auto defs = def->look((elem / Ident)->location());

          if (!defs.empty())
            def = defs.front();
        }
      }

      // Reify the type to get a ClassId.
      auto classid = reify_type(type_node, subst);

      // Convert NewArgs to Args, ordered by field position in the class.
      Node args = Args;

      if (def == ClassDef)
      {
        for (auto& f : *(def / ClassBody))
        {
          if (f != FieldDef)
            continue;

          auto field_name = (f / Ident)->location().view();

          for (auto& na : *newargs)
          {
            if ((na / Ident)->location().view() == field_name)
            {
              args << (Arg << ArgCopy << clone(na->at(1)));
              break;
            }
          }
        }
      }

      // Fallback: if we couldn't match fields, just use NewArgs order.
      if (args->empty())
      {
        for (auto& na : *newargs)
          args << (Arg << ArgCopy << clone(na->at(1)));
      }

      n->erase(n->begin(), n->end());
      n << dst << classid << args;
    }

    void reify_lookup(Node& n, const NodeMap<Node>& call_subst)
    {
      auto dst = n / LocalId;
      auto src = n / Rhs;
      auto hand = n / Lhs;
      auto ident = n / Ident;
      auto typeargs = n / TypeArgs;
      auto arity_node = n / Int;

      // Build method ID: "name::arity[::ref]::index"
      auto name = std::string(ident->location().view());
      auto arity_str = std::string(arity_node->location().view());
      auto base_id =
        std::format("{}::{}{}", name, arity_str, hand == Lhs ? "::ref" : "");

      // Find or create a reification index for these resolved type arguments.
      auto index = find_method_index(base_id, typeargs, call_subst);
      auto method_id_str = std::format("{}::{}", base_id, index);

      // Parse arity.
      size_t arity = 0;
      std::from_chars(
        arity_str.data(), arity_str.data() + arity_str.size(), arity);

      // Record this method invocation for method registration.
      method_invocations.push_back(
        {method_id_str,
         name,
         arity,
         hand->type(),
         clone(typeargs),
         call_subst});

      // Register this new MI on all existing class reifications.
      auto& mi = method_invocations.back();

      for (auto& [def, reifications] : map)
      {
        if (def != ClassDef)
          continue;

        for (auto& r : reifications)
        {
          if (r.reification)
            register_method(mi, r);
        }
      }

      auto mid = MethodId ^ method_id_str;

      n->erase(n->begin(), n->end());
      n << dst << src << mid;
    }

    // Register a single MethodInvocation on a single class Reification.
    // If the class has a matching function, reify it and add a Method entry.
    void register_method(const MethodInvocation& mi, Reification& r)
    {
      assert(r.def == ClassDef);

      auto mid_node = MethodId ^ mi.method_id;

      for (auto& f : *(r.def / ClassBody))
      {
        if (f != Function)
          continue;

        if ((f / Ident)->location().view() != mi.name)
          continue;

        if ((f / Lhs)->type() != mi.hand)
          continue;

        if ((f / Params)->size() != mi.arity)
          continue;

        auto func_tps = f / TypeParams;

        if (func_tps->size() != mi.typeargs->size())
          continue;

        // Build func_subst: class subst + method TypeParams -> resolved
        // TypeArgs.  Resolve TypeArgs through both call-site and class
        // substitution contexts (class subst takes priority for class
        // TypeParams).
        NodeMap<Node> combined = mi.call_subst;

        for (auto& [k, v] : r.subst)
          combined.insert_or_assign(k, v);

        NodeMap<Node> func_subst = r.subst;

        for (size_t i = 0; i < func_tps->size(); i++)
        {
          auto ta = mi.typeargs->at(i);
          Node resolved = (ta == Type) ? reify_type(ta, combined) : Dyn;
          func_subst[func_tps->at(i)] = resolved;
        }

        auto funcid = find_or_push(f, func_subst);

        // Check if this Method entry already exists.
        auto methods = r.reification / Methods;
        bool already = false;

        for (auto& existing : *methods)
        {
          if (
            ((existing / MethodId)->location().view() == mi.method_id) &&
            ((existing / FunctionId)->location().view() ==
             funcid->location().view()))
          {
            already = true;
            break;
          }
        }

        if (!already)
          methods << (Method << clone(mid_node) << funcid);
      }
    }

    void reify_ffi(Node& n, Reification& r)
    {
      auto sym_id = n / SymbolId;
      auto sym_name = sym_id->location();

      // Walk up from the function definition to find the Lib that defines
      // this symbol.
      auto def = r.def;
      auto parent = def->parent(ClassDef);

      while (parent)
      {
        for (auto& child : *(parent / ClassBody))
        {
          if (child != Lib)
            continue;

          for (auto& sym : *(child / Symbols))
          {
            if ((sym / SymbolId)->location() == sym_name)
            {
              // Found the matching symbol in this Lib.
              // Get or create the reified Lib.
              auto lib_loc = (child / String)->location();
              auto find = libs.find(lib_loc);
              Node reified_lib;

              if (find == libs.end())
              {
                reified_lib = Lib << clone(child / String) << Symbols;
                libs[lib_loc] = reified_lib;
              }
              else
              {
                reified_lib = find->second;
              }

              // Reify the types in the symbol.
              Node ffi_params = FFIParams;

              for (auto& p : *(sym / FFIParams))
                ffi_params << reify_type(p, r.subst);

              auto ret_type = reify_type(sym / Type, r.subst);

              // Check if this symbol has already been added.
              auto reified_symbols = reified_lib / Symbols;
              bool already_added = false;

              for (auto& existing : *reified_symbols)
              {
                if ((existing / SymbolId)->location() != sym_name)
                  continue;

                already_added = true;

                // Verify the existing declaration is consistent.
                auto lhs_a = (existing / Lhs)->location().view();
                auto lhs_b = (sym / Lhs)->location().view();
                auto rhs_a = (existing / Rhs)->location().view();
                auto rhs_b = (sym / Rhs)->location().view();
                auto existing_params = existing / FFIParams;
                Node existing_ret = existing->back();

                if (
                  (lhs_a != lhs_b) || (rhs_a != rhs_b) ||
                  !Subtype(existing_ret, ret_type) ||
                  !Subtype(ret_type, existing_ret) ||
                  !std::equal(
                    existing_params->begin(),
                    existing_params->end(),
                    ffi_params->begin(),
                    ffi_params->end(),
                    [&](auto& ep, auto& np) {
                      return Subtype(ep, np) && Subtype(np, ep);
                    }))
                {
                  n->parent()->replace(
                    n,
                    err(sym_id, "Conflicting FFI declarations")
                      << errmsg("Here:") << errloc(existing / SymbolId)
                      << errmsg("And here:") << errloc(sym_id));
                  return;
                }
                break;
              }

              if (!already_added)
              {
                reified_symbols
                  << (Symbol << clone(sym / SymbolId) << clone(sym / Lhs)
                             << clone(sym / Rhs) << clone(sym / Vararg)
                             << ffi_params << ret_type);
              }

              return;
            }
          }
        }

        parent = parent->parent(ClassDef);
      }
    }

    Node reify_when(Node& n, Reification& r)
    {
      return WhenDyn << (n / LocalId) << (n / Rhs) << (n / Args)
                     << (Cown << reify_type(n / Type, r.subst));
    }

    // Given a TypeName or FuncName and a substitution map, find or create a
    // reification and return the ClassId, TypeId, or FunctionId. The accept
    // function is used to filter the final definition, such as looking for a
    // function with a specific arity and handedness.
    template<typename F>
    Node get_reification(const Node& name, const NodeMap<Node>& subst, F accept)
    {
      assert(name->in({TypeName, FuncName}));
      Node def = top;

      // Navigate the fully qualified name from Top, collecting TypeParam
      // substitutions from TypeArgs along the way.
      // r.subst only contains entries for TypeParams encountered during
      // navigation (the def's own params), not the caller's context.
      // resolve_subst combines both for resolving TypeArg references.
      Reification r{top, {}, {}, {}};
      NodeMap<Node> resolve_subst = subst;

      for (auto it = name->begin(); it != name->end(); ++it)
      {
        auto& elem = *it;
        assert(elem == NameElement);
        auto ident = elem / Ident;
        auto ta = elem / TypeArgs;
        bool is_last = (it + 1 == name->end());

        auto defs = def->look(ident->location());

        if (defs.empty())
        {
          if (def == Top)
            return err(elem, "No top-level definition found");

          return err(elem, "Identifier not found")
            << errmsg("Resolving here:") << errloc(def / Ident);
        }

        if (is_last)
        {
          // If the definition is a TypeParam, look it up in the substitution
          // map and reify the substituted type directly.
          for (auto& d : defs)
          {
            if (d == TypeParam)
            {
              auto find = resolve_subst.find(d);

              if (find != resolve_subst.end())
                return reify_type(find->second, resolve_subst);

              return err(elem, "TypeParam has no substitution");
            }
          }

          // Use the accept filter to find the right def.
          bool found = false;

          for (auto& d : defs)
          {
            if (accept(d))
            {
              def = d;
              found = true;
              break;
            }
          }

          if (!found)
          {
            return err(elem, "No matching definition found")
              << errmsg("Resolving here:") << errloc(defs.front() / Ident);
          }
        }
        else
        {
          // Intermediate elements must resolve to a scope (ClassDef).
          def = defs.front();

          if (def == TypeParam)
          {
            // Look up the TypeParam in the substitution map and resolve
            // through the substituted type to find the ClassDef.
            auto find = resolve_subst.find(def);

            if (find == resolve_subst.end())
              return err(elem, "TypeParam has no substitution");

            auto sub = find->second;

            // Unwrap Type node.
            if (sub == Type)
              sub = sub->front();

            if (sub != TypeName)
            {
              return err(
                elem, "TypeParam substitution must be a type name here");
            }

            // Navigate from the substituted TypeName to find the ClassDef.
            def = top;

            for (auto& se : *sub)
            {
              auto si = se / Ident;
              auto sta = se / TypeArgs;
              auto sdefs = def->look(si->location());

              if (sdefs.empty())
                return err(se, "Definition not found in TypeParam resolution");

              def = sdefs.front();

              if (!sta->empty())
              {
                auto stps = def / TypeParams;

                for (size_t i = 0; i < stps->size(); i++)
                {
                  r.subst[stps->at(i)] = sta->at(i);
                  resolve_subst[stps->at(i)] = sta->at(i);
                }
              }
            }

            if (def != ClassDef)
            {
              return err(
                elem,
                "TypeParam substitution must resolve to a class for "
                "intermediate navigation");
            }
          }
          else if (def != ClassDef)
          {
            return err(elem, "Intermediate name must be a class")
              << errmsg("Resolving here:") << errloc(def / Ident);
          }
        }

        // Build substitution from TypeArgs when provided.
        auto tps = def / TypeParams;

        if (!ta->empty())
        {
          if (ta->size() != tps->size())
          {
            return err(
                     elem,
                     std::format(
                       "Expected {} type arguments, got {}",
                       tps->size(),
                       ta->size()))
              << errmsg("Resolving here:") << errloc(def / Ident);
          }

          for (size_t i = 0; i < tps->size(); i++)
          {
            // Substitute any TypeParam references in the TypeArg using the
            // full resolution context, to avoid self-referential cycles.
            auto arg = ta->at(i);
            auto resolved = resolve_typearg(arg, resolve_subst);
            r.subst[tps->at(i)] = resolved;
            resolve_subst[tps->at(i)] = resolved;
          }
        }
      }

      // Shapes are treated as dynamic types. No reification needed.
      if ((def == ClassDef) && ((def / Shape) == Shape))
        return Dyn;

      return find_or_push(def, std::move(r.subst));
    }

    Node make_id(const Node& def, size_t index, const NodeMap<Node>& subst)
    {
      if (def->parent(ClassDef) == builtin)
      {
        // Check for a bare primitive type.
        auto find = primitive_types.find((def / Ident)->location().view());

        if (find != primitive_types.end())
          return find->second;

        // Check for a wrapper type (array[T], cown[T], ref[T]).
        auto wrap_find = wrapper_types.find((def / Ident)->location().view());

        if (wrap_find != wrapper_types.end())
        {
          auto tps = def / TypeParams;
          assert(tps->size() == 1);
          auto tp_find = subst.find(tps->at(0));
          Node elem_type =
            (tp_find != subst.end()) ? reify_type(tp_find->second, subst) : Dyn;
          return Node(wrap_find->second) << elem_type;
        }
      }

      // Identifiers take the form `a::b::c::3`.
      assert(def->in({ClassDef, TypeAlias, Function}));
      auto id = std::string((def / Ident)->location().view());
      auto parent = def->parent({Top, ClassDef, TypeAlias, Function});

      while (parent && parent != Top)
      {
        id = std::format("{}::{}", (parent / Ident)->location().view(), id);
        parent = parent->parent({Top, ClassDef, TypeAlias, Function});
      }

      if (def == Function)
      {
        // A function adds arity and handedness.
        id = std::format(
          "{}::{}{}",
          id,
          (def / Params)->size(),
          (def / Lhs) == Lhs ? "::ref" : "");
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
    PassDef p{"reify", wfIR, dir::bottomup, {}};

    p.pre([=](auto top) {
      Reifier().run(top);
      return 0;
    });

    return p;
  }
}
