#include "../lang.h"
#include "../subtype.h"

namespace vc
{
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
  };

  // Primitive types nested under _builtin::ffi.
  const std::map<std::string_view, Node> ffi_primitive_types = {
    {"ptr", Ptr},
    {"callback", Callback},
  };

  struct Reifier
  {
    Reifier() {}

    // Check if a def's name is in the primitive or ffi_primitive maps.
    bool is_any_primitive(const Node& def) const
    {
      auto name = (def / Ident)->location().view();
      return primitive_types.find(name) != primitive_types.end() ||
        ffi_primitive_types.find(name) != ffi_primitive_types.end();
    }

    // Check if a def is transitively under the _builtin scope.
    bool is_under_builtin(const Node& def) const
    {
      auto parent = def->parent(ClassDef);

      while (parent)
      {
        if (parent == builtin)
          return true;
        parent = parent->parent(ClassDef);
      }

      return false;
    }

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

      // Resolve shapes: each shape becomes a Type node mapping its TypeId
      // to a Union of all reified concrete classes that satisfy it.
      {
        SequentCtx ctx{top, {}, {}};

        for (auto& key : map_order)
        {
          for (auto& r : map[key])
          {
            if (r.def != ClassDef || (r.def / Shape) != Shape)
              continue;

            assert(r.resolved_name);
            Node union_node = Union;

            for (auto& ckey : map_order)
            {
              for (auto& cr : map[ckey])
              {
                if (cr.def != ClassDef || (cr.def / Shape) == Shape)
                  continue;
                if (!cr.resolved_name)
                  continue;

                if (check_shape_subtype(ctx, cr.resolved_name, r.resolved_name))
                  union_node << clone(cr.id);
              }
            }

            if (union_node->empty())
              r.reification = Type << clone(r.id) << Dyn;
            else if (union_node->size() == 1)
              r.reification = Type << clone(r.id) << union_node->front();
            else
              r.reification = Type << clone(r.id) << union_node;
          }
        }
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
      // Iterate in insertion order (not pointer order) for determinism.
      for (auto& key : map_order)
        for (auto& r : map[key])
          top << r.reification;

      // Add reified libraries.
      for (auto& [_, lib] : libs)
        top << lib;

      // Add any errors collected during reification.
      for (auto& e : errors)
        top << e;
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
      Node resolved_name; // Resolved TypeName for shape checking
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
      Nodes receivers; // reified type ids of possible receivers; empty = all
    };

    Node top;
    Node builtin;
    NodeMap<std::deque<Reification>> map;
    std::vector<Node> map_order;
    std::vector<Reification*> worklist;
    std::map<Location, Node> libs;
    std::set<Node> processed_initfini;
    Nodes errors;
    std::vector<MethodInvocation> method_invocations;
    std::map<std::string, std::vector<std::vector<Node>>> method_index;

    // Per-function local type map: LocalId location -> reified type.
    // Populated during reify_function, used by reify_lookup.
    std::map<Location, Node> local_types;

    // Extract individual type ids from a reified type. For Union, extracts
    // each member. For Dyn, returns empty (meaning all classes). For
    // ClassId/primitive, returns just that type.
    Nodes extract_receivers(const Node& reified_type)
    {
      // Dyn means "all classes". TypeId means a shape type whose concrete
      // implementations aren't resolved yet (post-worklist), so treat as all.
      if (!reified_type || (reified_type == Dyn) || (reified_type == TypeId))
        return {};

      if (reified_type == Union)
      {
        Nodes result;

        for (auto& child : *reified_type)
        {
          if (child == Dyn)
            return {}; // union containing Dyn = all

          result.push_back(clone(child));
        }

        return result;
      }

      return {clone(reified_type)};
    }

    // Check if a MethodInvocation targets a specific class reification.
    bool mi_targets(const MethodInvocation& mi, Node class_id)
    {
      if (mi.receivers.empty())
        return true; // all classes

      for (auto r : mi.receivers)
      {
        if (class_id->equals(r))
          return true;
      }

      return false;
    }

    // Resolve a TypeArg through the current substitution map. If the TypeArg
    // is a Type wrapping a TypeName that resolves to a TypeParam in the subst,
    // return the substituted value. Otherwise, return the original TypeArg.
    Node resolve_typearg(const Node& arg, const NodeMap<Node>& subst)
    {
      auto inner = arg;

      // Unwrap Type node.
      if (inner == Type)
        inner = inner->front();

      if (inner == Union)
      {
        Node r = Union;

        for (auto& child : *inner)
          r << resolve_typearg(child, subst);

        return r;
      }

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

      if (def == TypeParam)
      {
        // It's a TypeParam. Look it up in the subst map.
        auto find = subst.find(def);

        if (find != subst.end())
          return find->second;

        // TypeParam not in subst — return Dyn to prevent self-referential
        // substitution cycles (where a TypeParam maps to a reference to
        // itself, causing infinite recursion in reify_type).
        return Type << Dyn;
      }

      // Not a bare TypeParam. Recursively resolve TypeParams in any nested
      // TypeArgs (e.g., array[T] → array[i32] when T → i32 is in subst).
      bool changed = false;
      Node resolved_name = TypeName;

      for (auto& elem : *inner)
      {
        auto ta = elem / TypeArgs;

        if (ta->empty())
        {
          resolved_name << clone(elem);
          continue;
        }

        Node new_ta = TypeArgs;

        for (auto& a : *ta)
        {
          auto resolved = resolve_typearg(a, subst);

          if (resolved != a)
            changed = true;

          new_ta << clone(resolved);
        }

        resolved_name << (NameElement << clone(elem / Ident) << new_ta);
      }

      if (!changed)
        return arg;

      // Re-wrap in Type if the original was wrapped.
      if (arg == Type)
        return Type << resolved_name;

      return resolved_name;
    }

    // Check whether two substitution maps are equivalent under invariance.
    bool subst_equal(const NodeMap<Node>& a, const NodeMap<Node>& b)
    {
      return std::equal(
        a.begin(), a.end(), b.begin(), b.end(), [&](auto& lhs, auto& rhs) {
          return (lhs.first == rhs.first) &&
            Subtype.invariant(top, lhs.second, rhs.second);
        });
    }

    // Compare two vectors of resolved types for invariant equality.
    bool typeargs_equal(const std::vector<Node>& a, const std::vector<Node>& b)
    {
      return std::equal(
        a.begin(), a.end(), b.begin(), b.end(), [&](auto& lhs, auto& rhs) {
          return Subtype.invariant(top, lhs, rhs);
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
    Node find_or_push(
      const Node& def, NodeMap<Node> subst, Node resolved_name = {})
    {
      auto it = map.find(def);
      bool is_new_key = (it == map.end());
      auto& r_vec = map[def];

      if (is_new_key)
        map_order.push_back(def);

      if (is_under_builtin(def))
      {
        auto name = (def / Ident)->location().view();

        if (
          primitive_types.find(name) != primitive_types.end() ||
          ffi_primitive_types.find(name) != ffi_primitive_types.end() ||
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

          r_vec.push_back(
            {def,
             std::move(subst),
             std::move(id),
             {},
             std::move(resolved_name)});
          worklist.push_back(&r_vec.back());
          return clone(r_vec.back().id);
        }
      }

      // All other defs: dedup using substitution map equality.
      // Only compare entries for TypeParams owned by this def — external
      // entries from enclosing scopes don't influence the reification
      // (they're already resolved) and can vary between call paths.
      auto own_tps = def / TypeParams;

      for (auto& existing : r_vec)
      {
        bool match = true;

        for (auto& tp : *own_tps)
        {
          auto a_it = existing.subst.find(tp);
          auto b_it = subst.find(tp);

          if (a_it == existing.subst.end() && b_it == subst.end())
            continue;

          if (a_it == existing.subst.end() || b_it == subst.end())
          {
            match = false;
            break;
          }

          if (!Subtype.invariant(top, a_it->second, b_it->second))
          {
            match = false;
            break;
          }
        }

        if (match)
          return clone(existing.id);
      }

      auto id = make_id(def, r_vec.size(), subst);

      r_vec.push_back(
        {def,
         std::move(subst),
         std::move(id),
         {},
         std::move(resolved_name)});
      worklist.push_back(&r_vec.back());
      return clone(r_vec.back().id);
    }

    void reify_class(Reification& r)
    {
      // Shape reification is handled post-worklist: build a Type node
      // mapping the shape's TypeId to a Union of matching concrete classes.
      if ((r.def / Shape) == Shape)
        return;

      // Skip creation if already reified (e.g., early call from
      // reify_make_callback). Method invocation registration below
      // still runs.
      if (!r.reification)
      {
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
      }

      // Register existing method invocations that target this class.
      for (auto& mi : method_invocations)
      {
        if (mi_targets(mi, r.id))
          register_method(mi, r);
      }
    }

    void reify_typealias(Reification& r)
    {
      // Store the reified type alias.
      r.reification = TypeAlias << r.id << reify_type(r.def / Type, r.subst);
    }

    bool reify_function(Reification& r)
    {
      // Clear per-function local type tracking.
      local_types.clear();

      // Reify the function signature.
      auto r_type = reify_type(r.def / Type, r.subst);
      Node params = Params;

      for (auto& p : *(r.def / Params))
      {
        auto p_type = reify_type(p / Type, r.subst);
        local_types[(p / Ident)->location()] = p_type;
        params << (Param << (LocalId ^ (p / Ident)) << p_type);
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
        Nodes splat_expand;

        // No structural changes required: CallDyn, math ops on existing
        // values.

        body->traverse([&](Node& n) {
          if (n == body)
            return true;

          if (n->in({Const, Convert}))
          {
            reify_primitive(n / Type);
            // Track type: Const/Convert produce the primitive type token.
            local_types[(n / LocalId)->location()] = clone(n / Type);
          }
          else if (n == ConstStr)
          {
            Node u8_type = TypeName
              << (NameElement << (Ident ^ "_builtin") << TypeArgs)
              << (NameElement << (Ident ^ "u8") << TypeArgs);
            ensure_array_reified(u8_type, {});
            local_types[(n / LocalId)->location()] = Array << clone(U8);
          }
          else if (n->in({Eq, Ne, Lt, Le, Gt, Ge}))
          {
            reify_primitive(Bool);
            local_types[(n / LocalId)->location()] = clone(Bool);
          }
          else if (n->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
          {
            reify_primitive(F64);
            local_types[(n / LocalId)->location()] = clone(F64);
          }
          else if (n == Bits)
          {
            reify_primitive(U64);
            local_types[(n / LocalId)->location()] = clone(U64);
          }
          else if (n == Len)
          {
            reify_primitive(USize);
            local_types[(n / LocalId)->location()] = clone(USize);
          }
          else if (n == MakePtr)
          {
            reify_primitive(Ptr);
            local_types[(n / LocalId)->location()] = clone(Ptr);
          }
          else if (n == MakeCallback)
          {
            reify_primitive(Callback);
            local_types[(n / LocalId)->location()] = clone(Callback);

            // Find the lambda's type and register its @callback method.
            // First check local_types (works when source was from New).
            auto src_loc = (n / Rhs)->location();
            auto src_it = local_types.find(src_loc);
            Node class_id;

            if (src_it != local_types.end() && (src_it->second == ClassId))
            {
              class_id = src_it->second;
            }
            else
            {
              // local_types doesn't have it (e.g., assigned via Call).
              // Trace back through Copy/Move to find the original source.
              auto body_node = n->parent();

              // Pass 1: follow Copy/Move chain to root source.
              auto trace_loc = src_loc;
              bool changed = true;

              while (changed)
              {
                changed = false;

                for (auto& stmt : *body_node)
                {
                  if (&stmt == &n)
                    break;

                  if (
                    stmt->in({Copy, Move}) &&
                    ((stmt / LocalId)->location() == trace_loc))
                  {
                    trace_loc = (stmt / Rhs)->location();
                    changed = true;
                  }
                }
              }

              // Pass 2: find the definition of the root source.
              Node call_enc;

              for (auto& stmt : *body_node)
              {
                if (&stmt == &n)
                  break;

                if (
                  stmt->in({New, Stack}) &&
                  ((stmt / LocalId)->location() == trace_loc))
                {
                  class_id = stmt / ClassId;
                }
                else if (
                  (stmt == Call) &&
                  ((stmt / LocalId)->location() == trace_loc))
                {
                  // Find the Function reification for this Call, then
                  // trigger reification of its enclosing ClassDef.
                  auto funcid_loc =
                    (stmt / FunctionId)->location().view();
                  NodeMap<Node> class_subst;

                  for (auto& key : map_order)
                  {
                    if (key != Function)
                      continue;

                    for (auto& reif : map[key])
                    {
                      if (
                        reif.id &&
                        (reif.id->location().view() == funcid_loc))
                      {
                        auto enc = reif.def->parent(ClassDef);

                        if (enc)
                        {
                          // Extract the class's TypeParam substitutions
                          // from the function's substitution context.
                          for (auto& tp : *(enc / TypeParams))
                          {
                            auto sit = reif.subst.find(tp);

                            if (sit != reif.subst.end())
                              class_subst[sit->first] =
                                clone(sit->second);
                          }

                          call_enc = enc;
                        }

                        break;
                      }
                    }

                    if (call_enc)
                      break;
                  }

                  // Trigger class reification AFTER the map_order loop
                  // to avoid iterator invalidation (find_or_push may
                  // append to map_order).
                  if (call_enc)
                    class_id =
                      find_or_push(call_enc, std::move(class_subst));
                }
              }
            }

            if (class_id)
              reify_make_callback(n, class_id);
            else
              n->parent()->replace(
                n,
                err(n, "make_callback: cannot determine lambda type"));
          }
          else if (n->in({CallbackPtr, FreeCallback}))
          {
            if (n == CallbackPtr)
            {
              reify_primitive(Ptr);
              local_types[(n / LocalId)->location()] = clone(Ptr);
            }
            else
            {
              reify_primitive(None);
              local_types[(n / LocalId)->location()] = clone(None);
            }
          }
          else if (n->in({AddExternal, RemoveExternal}))
          {
            reify_primitive(None);
            local_types[(n / LocalId)->location()] = clone(None);
          }
          else if (n == RegisterExternalNotify)
          {
            reify_primitive(None);
            local_types[(n / LocalId)->location()] = clone(None);
          }
          else if (n->in({Copy, Move}))
          {
            // Propagate type from source to destination.
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end())
              local_types[(n / LocalId)->location()] =
                clone(src_it->second);

            // Elide dead Copies.
            if (
              (n == Copy) &&
              used_locs.find((n / LocalId)->location()) == used_locs.end())
              remove.push_back(n);
          }
          else if (n == RegisterRef)
          {
            // RegisterRef: dst = &src. Result type is Ref << type(src).
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end())
            {
              ensure_ref_reified(src_it->second);
              local_types[(n / LocalId)->location()] =
                Node(Ref) << clone(src_it->second);
            }
          }
          else if (n == FieldRef)
          {
            // FieldRef: dst = &(arg.field). Result type is
            // Ref << reified field type.
            auto obj_loc = (n / Arg / Rhs)->location();
            auto obj_it = local_types.find(obj_loc);

            if (obj_it != local_types.end() && (obj_it->second == ClassId))
            {
              auto ft = find_field_type(obj_it->second, n / FieldId);

              if (ft)
              {
                ensure_ref_reified(ft);
                local_types[(n / LocalId)->location()] =
                  Node(Ref) << ft;
              }
            }
          }
          else if (n->in({ArrayRef, ArrayRefConst}))
          {
            // ArrayRef/ArrayRefConst: dst = &(arr[i]). Result type is
            // Ref << element type.
            auto arr_loc = (n / Arg / Rhs)->location();
            auto arr_it = local_types.find(arr_loc);

            if (arr_it != local_types.end())
            {
              Node elem;

              if (arr_it->second == TupleType && n == ArrayRefConst)
              {
                // TupleType is a peer of Array: extract per-element type
                // by constant index.
                auto idx = from_chars_sep_v<size_t>(n / Rhs);

                if (idx < arr_it->second->size())
                  elem = clone(arr_it->second->at(idx));
                else
                  elem = Dyn;
              }
              else if (arr_it->second == TupleType)
              {
                // Runtime-indexed access on a TupleType: element type is
                // unknown at compile time.
                elem = Dyn;
              }
              else if (arr_it->second == Array)
              {
                elem = clone(arr_it->second->front());
              }

              if (elem)
              {
                ensure_ref_reified(elem);
                local_types[(n / LocalId)->location()] =
                  Node(Ref) << elem;
              }
            }
          }
          else if (n == ArrayRefFromEnd)
          {
            // Compute element type and collect for post-traversal expansion.
            auto arr_loc = (n / Arg / Rhs)->location();
            auto arr_it = local_types.find(arr_loc);

            if (arr_it != local_types.end() && arr_it->second == TupleType)
            {
              auto from_end = from_chars_sep_v<size_t>(n / Rhs);
              auto arity = arr_it->second->size();

              if (from_end >= 1 && from_end <= arity)
              {
                auto real_idx = arity - from_end;
                auto elem = clone(arr_it->second->at(real_idx));
                ensure_ref_reified(elem);
                local_types[(n / LocalId)->location()] =
                  Node(Ref) << elem;
              }
            }

            splat_expand.push_back(n);
          }
          else if (n == SplatOp)
          {
            // Compute result type and collect for post-traversal expansion.
            auto arr_loc = (n / Arg / Rhs)->location();
            auto arr_it = local_types.find(arr_loc);

            if (arr_it != local_types.end() && arr_it->second == TupleType)
            {
              auto before = from_chars_sep_v<size_t>(n / Lhs);
              auto after = from_chars_sep_v<size_t>(n / Rhs);
              auto arity = arr_it->second->size();

              if (before + after <= arity)
              {
                auto remaining = arity - before - after;

                if (remaining == 0)
                {
                  reify_primitive(clone(None));
                  local_types[(n / LocalId)->location()] = clone(None);
                }
                else if (remaining == 1)
                {
                  local_types[(n / LocalId)->location()] =
                    clone(arr_it->second->at(before));
                }
                else
                {
                  Node ttype = TupleType;

                  for (size_t i = before; i < before + remaining; i++)
                    ttype << clone(arr_it->second->at(i));

                  local_types[(n / LocalId)->location()] = clone(ttype);
                }
              }
            }

            splat_expand.push_back(n);
          }
          else if (n == Load)
          {
            // Load: dst = *src. Unwrap Ref to get inner type.
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end() && (src_it->second == Ref))
              local_types[(n / LocalId)->location()] =
                clone(src_it->second->front());
          }
          else if (n == Store)
          {
            // Store: dst = old *src, *src = arg. Result is old value type.
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end() && (src_it->second == Ref))
              local_types[(n / LocalId)->location()] =
                clone(src_it->second->front());
          }
          else if (n == Var)
          {
            vars << (LocalId ^ (n / Ident));
            remove.push_back(n);
          }
          else if (n->in({New, Stack}))
          {
            // Save the type before reify_new transforms the node.
            auto new_type = reify_type(n / Type, r.subst);
            reify_new(n, r.subst);
            // After reify_new, dst is first child.
            local_types[(n / LocalId)->location()] = new_type;
          }
          else if (n == Lookup)
          {
            reify_lookup(n, r.subst);
          }
          else if (n == Call)
          {
            reify_call(n, r.subst);
          }
          else if (n == NewArray)
          {
            auto arr_type = Array << reify_type(n / Type, r.subst);
            local_types[(n / LocalId)->location()] = clone(arr_type);
            n / Type = arr_type;
          }
          else if (n == NewArrayConst)
          {
            auto inner = reify_type(n / Type, r.subst);

            if (inner == TupleType)
            {
              // TupleType is a peer of Array, not wrapped in it.
              local_types[(n / LocalId)->location()] = clone(inner);
              n / Type = inner;
            }
            else
            {
              // Save original Type before reification overwrites it.
              // ensure_array_reified needs the TypeName form for subst.
              auto orig_type = clone(n / Type);
              auto arr_type = Array << inner;
              local_types[(n / LocalId)->location()] = clone(arr_type);
              n / Type = arr_type;

              // For array literals, trigger reification of the array class
              // so method invocations (size, apply) can be resolved.
              auto loc_view = (n / LocalId)->location().view();
              bool is_array_lit =
                loc_view.size() >= 5 && loc_view.substr(0, 5) == "array";

              if (is_array_lit)
                ensure_array_reified(orig_type, r.subst);
            }
          }
          else if (n == FFI)
          {
            reify_ffi(n, r);
          }
          else if (n == When)
          {
            n->parent()->replace(n, reify_when(n, r));
          }
          else if (n == Typetest)
          {
            n / Type = reify_type(n / Type, r.subst);
          }

          return false;
        });

        for (auto& n : remove)
          n->parent()->replace(n);

        // Expand ArrayRefFromEnd and SplatOp nodes.
        for (auto& n : splat_expand)
        {
          if (n == ArrayRefFromEnd)
          {
            // Convert to ArrayRefConst with computed index.
            auto arr_loc = (n / Arg / Rhs)->location();
            auto arr_it = local_types.find(arr_loc);

            if (arr_it != local_types.end() && arr_it->second == TupleType)
            {
              auto from_end = from_chars_sep_v<size_t>(n / Rhs);
              auto arity = arr_it->second->size();
              auto real_idx = arity - from_end;

              Node replacement = ArrayRefConst
                << clone(n / LocalId)
                << clone(n / Arg)
                << (Int ^ std::to_string(real_idx));

              body->replace(n, replacement);
            }
            else
            {
              assert(false && "ArrayRefFromEnd source must be TupleType");
            }
          }
          else if (n == SplatOp)
          {
            auto arr_loc = (n / Arg / Rhs)->location();
            auto arr_it = local_types.find(arr_loc);

            if (arr_it != local_types.end() && arr_it->second == TupleType)
            {
              auto before = from_chars_sep_v<size_t>(n / Lhs);
              auto after = from_chars_sep_v<size_t>(n / Rhs);
              auto arity = arr_it->second->size();

              if (before + after > arity)
              {
                body->replace(
                  n,
                  err(
                    n,
                    "tuple has " + std::to_string(arity) +
                      " elements, but destructuring requires at least " +
                      std::to_string(before + after)));
                continue;
              }

              auto remaining = arity - before - after;
              auto dst_loc = (n / LocalId)->location();

              if (remaining == 0)
              {
                // No remaining elements: produce a None constant.
                Node replacement = Const
                  << (LocalId ^ dst_loc)
                  << clone(None)
                  << clone(None);

                body->replace(n, replacement);
              }
              else if (remaining == 1)
              {
                // One element: ArrayRefConst + Load.
                auto ref_loc = top->fresh(Location("splat"));

                Node aref = ArrayRefConst
                  << (LocalId ^ ref_loc)
                  << clone(n / Arg)
                  << (Int ^ std::to_string(before));

                Node load = Load
                  << (LocalId ^ dst_loc)
                  << (LocalId ^ ref_loc);

                Nodes replacements = {aref, load};
                auto it = body->find(n);
                auto pos = body->erase(it, std::next(it));
                body->insert(pos, replacements.begin(), replacements.end());
              }
              else
              {
                // Two or more elements: create a new tuple.
                Node ttype = TupleType;

                for (size_t i = before; i < before + remaining; i++)
                  ttype << clone(arr_it->second->at(i));

                Nodes replacements;

                // NewArrayConst to allocate the tuple.
                replacements.push_back(
                  NewArrayConst
                    << (LocalId ^ dst_loc)
                    << clone(ttype)
                    << (Int ^ std::to_string(remaining)));

                // Copy each element from source to destination.
                for (size_t i = 0; i < remaining; i++)
                {
                  auto src_ref = top->fresh(Location("splat"));
                  auto val_loc = top->fresh(Location("splat"));
                  auto dst_ref = top->fresh(Location("splat"));
                  auto old_val = top->fresh(Location("splat"));

                  // Get ref to source element.
                  replacements.push_back(
                    ArrayRefConst
                      << (LocalId ^ src_ref)
                      << clone(n / Arg)
                      << (Int ^ std::to_string(before + i)));

                  // Load source value.
                  replacements.push_back(
                    Load
                      << (LocalId ^ val_loc)
                      << (LocalId ^ src_ref));

                  // Get ref to destination element.
                  replacements.push_back(
                    ArrayRefConst
                      << (LocalId ^ dst_ref)
                      << (Arg << ArgCopy << (LocalId ^ dst_loc))
                      << (Int ^ std::to_string(i)));

                  // Store value into destination.
                  replacements.push_back(
                    Store
                      << (LocalId ^ old_val)
                      << (LocalId ^ dst_ref)
                      << (Arg << ArgCopy << (LocalId ^ val_loc)));
                }

                auto it = body->find(n);
                auto pos = body->erase(it, std::next(it));
                body->insert(pos, replacements.begin(), replacements.end());
              }
            }
            else
            {
              assert(false && "SplatOp source must be TupleType");
            }
          }
        }

        // Reify the Type child of Raise terminators.
        auto term = l / Return;

        if (term == Raise)
          term / Type = reify_type(term / Type, r.subst);
      }

      if ((r.def / Lhs) == Once)
      {
        r.reification =
          FuncOnce << r.id << params << r_type << vars << labels;
      }
      else
      {
        r.reification = Func << r.id << params << r_type << vars << labels;
      }

      // If this is an init function, ensure the return value's class has
      // @callback registered so the runtime can call it as fini.
      if (
        r.def->parent(Symbols) &&
        ((r.def / Ident)->location().view() == "init"))
      {
        // Find the last Return terminator's local.
        for (auto& l : *labels)
        {
          auto term = l / Return;

          if (term != Return)
            continue;

          auto ret_loc = (term / LocalId)->location();
          auto body_node = l / Body;

          // Trace through Copy/Move to find the original source.
          bool changed = true;

          while (changed)
          {
            changed = false;

            for (auto& stmt : *body_node)
            {
              if (
                stmt->in({Copy, Move}) &&
                ((stmt / LocalId)->location() == ret_loc))
              {
                ret_loc = (stmt / Rhs)->location();
                changed = true;
                break;
              }
            }
          }

          // Check local_types first (works for New/Stack).
          auto it = local_types.find(ret_loc);

          if (it != local_types.end() && (it->second->type() == ClassId))
          {
            ensure_callback_method(it->second);
            break;
          }

          // Check if the return value comes from a Call (e.g., create).
          for (auto& stmt : *body_node)
          {
            if (
              (stmt == Call) &&
              ((stmt / LocalId)->location() == ret_loc))
            {
              // Find the Function reification and its enclosing ClassDef.
              auto funcid_loc = (stmt / FunctionId)->location().view();
              Node call_enc;
              NodeMap<Node> class_subst;

              for (auto& key : map_order)
              {
                if (key != Function)
                  continue;

                for (auto& reif : map[key])
                {
                  if (
                    reif.id &&
                    (reif.id->location().view() == funcid_loc))
                  {
                    auto enc = reif.def->parent(ClassDef);

                    if (enc)
                    {
                      for (auto& tp : *(enc / TypeParams))
                      {
                        auto sit = reif.subst.find(tp);

                        if (sit != reif.subst.end())
                          class_subst[sit->first] = clone(sit->second);
                      }

                      call_enc = enc;
                    }

                    break;
                  }
                }

                if (call_enc)
                  break;
              }

              if (call_enc)
              {
                auto class_id = find_or_push(call_enc, std::move(class_subst));
                ensure_callback_method(class_id);
              }

              break;
            }
          }

          break;
        }
      }

      return true;
    }

    // Turn a type into an IR type. The IR doesn't have intersection types,
    // structural types, or tuple types.
    Node reify_type(const Node& type, const NodeMap<Node>& subst)
    {
      if (type == Type)
        return reify_type(type->front(), subst);

      // Already-reified IR type (e.g., from Dyn fallback for missing
      // TypeArgs). Return as-is.
      if (type == Dyn)
        return Dyn;

      // Preserve TupleType with reified element types.
      if (type == TupleType)
      {
        Node r = TupleType;

        for (auto& child : *type)
          r << reify_type(child, subst);

        return r;
      }

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
      // Check flat primitives (_builtin::name).
      for (auto& [k, v] : primitive_types)
      {
        if (type->type() != v->type())
          continue;

        auto defs = builtin->look(Location(std::string(k)));
        assert(defs.size() == 1);

        Node prim_name = TypeName
          << (NameElement << (Ident ^ "_builtin") << TypeArgs)
          << (NameElement << (Ident ^ std::string(k)) << TypeArgs);

        find_or_push(defs.front(), {}, prim_name);
        return;
      }

      // Check ffi primitives (_builtin::ffi::name).
      for (auto& [k, v] : ffi_primitive_types)
      {
        if (type->type() != v->type())
          continue;

        auto ffi_defs = builtin->look(Location("ffi"));
        assert(ffi_defs.size() == 1);
        auto ffi_def = ffi_defs.front();

        auto defs = ffi_def->lookdown(Location(std::string(k)));
        assert(defs.size() == 1);

        Node prim_name = TypeName
          << (NameElement << (Ident ^ "_builtin") << TypeArgs)
          << (NameElement << (Ident ^ "ffi") << TypeArgs)
          << (NameElement << (Ident ^ std::string(k)) << TypeArgs);

        find_or_push(defs.front(), {}, prim_name);
        return;
      }
    }

    // Look up a field's reified type from a ClassId.  Finds the Reification
    // matching `classid`, locates the FieldDef by name, and reifies the field
    // type using the class's substitution map.
    Node find_field_type(Node classid, const Node& field_id)
    {
      auto field_name = field_id->location().view();

      for (auto& key : map_order)
      {
        for (auto& r : map[key])
        {
          if (!r.id->equals(classid) || (r.def != ClassDef))
            continue;

          for (auto& f : *(r.def / ClassBody))
          {
            if (f != FieldDef)
              continue;

            if ((f / Ident)->location().view() != field_name)
              continue;

            return reify_type(f / Type, r.subst);
          }

          return {};
        }
      }

      return {};
    }

    // Ensure that a Ref wrapper primitive with the given inner IR type is
    // reified.  Checks for an existing entry by structural id equality and
    // creates one via the worklist if absent.
    void ensure_ref_reified(const Node& inner_ir_type)
    {
      if (!inner_ir_type || (inner_ir_type == Dyn))
        return;

      auto ref_defs = builtin->look(Location("ref"));
      assert(!ref_defs.empty());
      auto ref_def = ref_defs.front();
      Node expected_id = Node(Ref) << clone(inner_ir_type);

      auto it = map.find(ref_def);
      bool is_new_key = (it == map.end());
      auto& r_vec = map[ref_def];

      if (is_new_key)
        map_order.push_back(ref_def);

      for (auto& existing : r_vec)
      {
        if (existing.id->equals(expected_id))
          return;
      }

      r_vec.push_back({ref_def, {}, std::move(expected_id), {}, {}});
      worklist.push_back(&r_vec.back());
    }

    // Ensure that the array wrapper class is reified for a given element type.
    // Called from array literal processing so that method invocations (size,
    // apply) have a class reification to bind against.
    void ensure_array_reified(
      const Node& elem_type, const NodeMap<Node>& outer_subst)
    {
      auto array_defs = builtin->look(Location("array"));
      assert(!array_defs.empty());
      auto array_def = array_defs.front();

      auto tps = array_def / TypeParams;
      assert(tps->size() == 1);

      // Build subst mapping the array's TypeParam T to the element type.
      // Include outer_subst so any TypeParam refs in elem_type are resolved.
      NodeMap<Node> subst = outer_subst;
      subst[tps->at(0)] = clone(elem_type);

      find_or_push(array_def, std::move(subst));
    }

    void reify_call(Node& call, const NodeMap<Node>& subst)
    {
      auto hand = (call / Lhs)->type();
      auto arity = (call / Args)->size();

      auto funcid = get_reification(call / FuncName, subst, [&](auto& def) {
        return (def == Function) && ((def / Params)->size() == arity) &&
          (((def / Lhs) == hand) ||
           ((def / Lhs) == Once && hand == Rhs));
      });

      if (!funcid || (funcid == Dyn))
      {
        // Can't resolve function — type arguments may need to be explicit.
        auto funcname = call / FuncName;
        call->parent()->replace(
          call,
          err(
            funcname,
            "Cannot resolve function — type arguments may need to be "
            "specified explicitly"));
        return;
      }

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
      auto arity = from_chars_sep_v<size_t>(arity_node);
      auto base_id =
        std::format("{}::{}{}", name, arity, hand == Lhs ? "::ref" : "");

      // Find or create a reification index for these resolved type arguments.
      auto index = find_method_index(base_id, typeargs, call_subst);
      auto method_id_str = std::format("{}::{}", base_id, index);

      // Determine receiver types from the source local's tracked type.
      Nodes receivers;
      auto src_it = local_types.find(src->location());

      if (src_it != local_types.end())
        receivers = extract_receivers(src_it->second);

      // Record this method invocation for method registration.
      method_invocations.push_back(
        {method_id_str,
         name,
         arity,
         hand->type(),
         clone(typeargs),
         call_subst,
         std::move(receivers)});

      // Register this new MI on existing class reifications that match.
      // Iterate via map_order (insertion order) rather than map (pointer order)
      // to ensure deterministic function reification ordering across runs.
      // Use index-based loop because register_method -> find_or_push can
      // push_back to map_order, invalidating range-for iterators.
      auto& mi = method_invocations.back();
      auto map_order_size = map_order.size();

      for (size_t i = 0; i < map_order_size; i++)
      {
        if (map_order[i] != ClassDef)
          continue;

        for (auto& r : map[map_order[i]])
        {
          if (r.reification && mi_targets(mi, r.id))
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

    // Core logic for registering @callback on a class. Returns true if
    // the callback method was successfully registered, false otherwise.
    // If match_count_out and has_generic_out are provided, they report
    // details about the apply method search.
    bool ensure_callback_method(
      const Node& class_id,
      size_t* match_count_out = nullptr,
      bool* has_generic_out = nullptr)
    {
      // Find the Reification for the lambda's class.
      auto class_id_loc = class_id->location().view();
      Reification* target_r = nullptr;

      for (auto& key : map_order)
      {
        if (key != ClassDef)
          continue;

        for (auto& r : map[key])
        {
          if (r.id && (r.id->location().view() == class_id_loc))
          {
            target_r = &r;
            break;
          }
        }

        if (target_r)
          break;
      }

      if (!target_r)
        return false;

      // Ensure the class has been reified (it may have just been
      // added to the worklist by find_or_push and not yet processed).
      if (!target_r->reification)
        reify_class(*target_r);

      // Scan the ClassDef for a unique non-generic `apply` method.
      Node found_func;
      size_t match_count = 0;
      bool has_generic = false;

      for (auto& f : *(target_r->def / ClassBody))
      {
        if (f != Function)
          continue;

        if ((f / Ident)->location().view() != "apply")
          continue;

        if ((f / Lhs)->type() != Rhs)
          continue;

        if (!((f / TypeParams)->empty()))
        {
          has_generic = true;
          continue;
        }

        found_func = f;
        match_count++;
      }

      if (match_count_out)
        *match_count_out = match_count;
      if (has_generic_out)
        *has_generic_out = has_generic;

      if (match_count != 1)
        return false;

      // Reify the apply function with the class's substitution context.
      auto funcid = find_or_push(found_func, target_r->subst);

      // Register the @callback Method on the class.
      auto methods = target_r->reification / Methods;
      auto mid_node = MethodId ^ "@callback";
      bool already = false;

      for (auto& existing : *methods)
      {
        if (
          ((existing / MethodId)->location().view() == "@callback") &&
          ((existing / FunctionId)->location().view() ==
           funcid->location().view()))
        {
          already = true;
          break;
        }
      }

      if (!already)
        methods << (Method << clone(mid_node) << funcid);

      return true;
    }

    void reify_make_callback(Node& n, const Node& class_id)
    {
      size_t match_count = 0;
      bool has_generic = false;

      if (ensure_callback_method(class_id, &match_count, &has_generic))
        return;

      if (match_count == 0)
      {
        auto msg = has_generic ?
          "make_callback requires a non-generic 'apply' method" :
          "make_callback requires a type with an 'apply' method";
        n->parent()->replace(n, err(n, msg));
        return;
      }

      if (match_count > 1)
      {
        n->parent()->replace(
          n,
          err(n, "make_callback requires exactly one 'apply' overload"));
        return;
      }
    }

    // Reify init functions from a source Lib onto a reified Lib.
    // Checks for duplicate init across multiple Lib definitions
    // for the same library (by string name).
    void reify_initfini(
      const Node& source_lib, Node& reified_lib, Reification& r)
    {
      // Skip if this source Lib node has already been processed.
      if (!processed_initfini.insert(source_lib).second)
        return;

      for (auto& child : *(source_lib / Symbols))
      {
        if (child != Function)
          continue;

        auto name = (child / Ident)->location().view();

        if (name != "init")
          continue;

        auto existing = reified_lib / InitFunc;

        if (existing != None)
        {
          // Already has an init — conflict error.
          auto msg = std::format(
            "Conflicting 'init' for library \"{}\"",
            (source_lib / String)->location().view());

          errors.push_back(
            err(child / Ident, msg)
            << errmsg("Previous declaration resolved here:")
            << errloc(existing));
          continue;
        }

        // Reify the init function.
        auto funcid = find_or_push(child, r.subst);
        reified_lib->replace(existing, clone(funcid));
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
            if (sym != Symbol)
              continue;

            if ((sym / SymbolId)->location() == sym_name)
            {
              // Found the matching symbol in this Lib.
              // Get or create the reified Lib.
              auto lib_loc = (child / String)->location();
              auto find = libs.find(lib_loc);
              Node reified_lib;

              if (find == libs.end())
              {
                reified_lib =
                  Lib << clone(child / String) << Symbols << None;
                libs[lib_loc] = reified_lib;
              }
              else
              {
                reified_lib = find->second;
              }

              // Reify init functions from all Lib definitions for this
              // library in the enclosing ClassDef.
              for (auto& lib_child : *(parent / ClassBody))
              {
                if (lib_child != Lib)
                  continue;

                if (
                  (lib_child / String)->location().view() !=
                  lib_loc.view())
                  continue;

                reify_initfini(lib_child, reified_lib, r);
              }

              // Reify the types in the symbol.
              Node ffi_params = FFIParams;

              for (auto& p : *(sym / FFIParams))
                ffi_params << reify_type(p, r.subst);

              auto ret_type = reify_type(sym / Type, r.subst);

              // Add the reified symbol. Duplicate detection and type
              // compatibility checking is done in the vbcc assignids pass.
              auto reified_symbols = reified_lib / Symbols;
              reified_symbols
                << (Symbol << clone(sym / SymbolId) << clone(sym / Lhs)
                           << clone(sym / Rhs) << clone(sym / Vararg)
                           << ffi_params << ret_type);

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
      Reification r{top, {}, {}, {}, {}};
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

            if (sub == Dyn)
              return err(
                elem,
                "Cannot resolve type — type arguments may need to be "
                "specified explicitly");

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
          else if (!def->in({ClassDef, Function}))
          {
            return err(elem, "Intermediate name must be a class or function")
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

      // Build a resolved TypeName with all TypeParam refs substituted.
      // This is stored on the Reification for use in shape checking.
      Node resolved_name;
      resolved_name = name->type();

      for (auto& elem : *name)
      {
        Node new_ta = TypeArgs;

        for (auto& a : *(elem / TypeArgs))
          new_ta << clone(resolve_typearg(a, resolve_subst));

        resolved_name
          << (NameElement << clone(elem / Ident) << new_ta);
      }

      // Shapes produce Dyn in function bodies (preserving method dispatch
      // behavior), but we record a map entry so the post-worklist phase
      // can build a Type << TypeId << Union of matching concrete classes.
      if ((def == ClassDef) && ((def / Shape) == Shape))
      {
        // _builtin::any is the universal shape — remains pure Dyn.
        if (
          (def->parent(ClassDef) == builtin) &&
          ((def / Ident)->location().view() == "any"))
          return Dyn;

        return find_or_push(def, std::move(r.subst), resolved_name);
      }

      return find_or_push(def, std::move(r.subst), resolved_name);
    }

    Node make_id(const Node& def, size_t index, const NodeMap<Node>& subst)
    {
      if (is_under_builtin(def) && (def == ClassDef))
      {
        // Check for a bare primitive type.
        auto find = primitive_types.find((def / Ident)->location().view());

        if (find != primitive_types.end())
          return find->second;

        // Check for an ffi primitive type.
        auto ffi_find =
          ffi_primitive_types.find((def / Ident)->location().view());

        if (ffi_find != ffi_primitive_types.end())
          return ffi_find->second;

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
      {
        if ((def / Shape) == Shape)
          return TypeId ^ id;
        return ClassId ^ id;
      }
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
