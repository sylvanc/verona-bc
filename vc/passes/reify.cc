#include "../lang.h"
#include "../subtype.h"
#include "../typevar.h"

#include <trieste/nodeworker.h>
#include <vbcc/irsubtype.h>

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

    bool contains_dyn(const Node& type) const
    {
      if (!type)
        return false;

      if (type == Dyn)
        return true;

      for (auto& child : *type)
      {
        if (contains_dyn(child))
          return true;
      }

      return false;
    }

    bool contains_typeid(const Node& type) const
    {
      if (!type)
        return false;

      if (type == TypeId)
        return true;

      for (auto& child : *type)
      {
        if (contains_typeid(child))
          return true;
      }

      return false;
    }

    bool is_nomatch_ir(const Node& type) const
    {
      return type && (type == ClassId) &&
        (type->location().view() == "_builtin::nomatch::0");
    }

    // True if `type` (typically a source-side Type node) contains a free
    // TypeVar leaf anywhere in its structure (Union, Isect, TupleType,
    // wrapper types, or inside a TypeName's TypeArgs). Distinct from
    // has_unresolved_type which also treats unbound TypeParam refs as
    // unresolved — here we want to detect ONLY inference unknowns
    // (TypeVar) that should be re-derived from label_exits.
    bool contains_typevar_leaf(const Node& type) const
    {
      if (!type)
        return false;

      if (type == TypeVar)
        return true;

      if (type->in({Type, Union, Isect, TupleType, Array, Ref, Cown}))
      {
        for (auto& c : *type)
          if (contains_typevar_leaf(c))
            return true;
      }

      if (type == TypeName)
      {
        for (auto& elem : *type)
          for (auto& arg : *(elem / TypeArgs))
            if (contains_typevar_leaf(arg))
              return true;
      }

      return false;
    }

    bool has_unresolved_type(const Node& type, const NodeMap<Node>& subst) const
    {
      std::set<Node> seen;
      return has_unresolved_type(type, subst, seen);
    }

    bool contains_typeparam_ref(const Node& type) const
    {
      if (!type)
        return false;

      if (type == Type)
        return contains_typeparam_ref(type->front());

      if (type == TypeVar)
        return true;

      if (type == TypeName)
      {
        auto def = top;

        for (auto it = type->begin(); it != type->end(); ++it)
        {
          auto& elem = *it;

          for (auto& arg : *(elem / TypeArgs))
          {
            if (contains_typeparam_ref(arg))
              return true;
          }

          auto defs = def->look((elem / Ident)->location());

          if (defs.empty())
            return false;

          if (defs.size() > 1 && (it + 1) != type->end())
          {
            auto next_ident = (*(it + 1)) / Ident;
            auto picked = defs.front();

            for (auto& d : defs)
            {
              if (!d->look(next_ident->location()).empty())
              {
                picked = d;
                break;
              }
            }

            def = picked;
          }
          else
          {
            def = defs.front();
          }

          if (def == TypeParam)
            return true;
        }

        return false;
      }

      for (auto& child : *type)
      {
        if (contains_typeparam_ref(child))
          return true;
      }

      return false;
    }

    bool has_unresolved_type(
      const Node& type, const NodeMap<Node>& subst, std::set<Node>& seen) const
    {
      if (!type)
        return false;

      if (type == Type)
        return has_unresolved_type(type->front(), subst, seen);

      if (type == TypeVar)
        return true;

      if (type == TypeName)
      {
        auto def = top;

        for (auto it = type->begin(); it != type->end(); ++it)
        {
          auto& elem = *it;

          for (auto& arg : *(elem / TypeArgs))
          {
            if (has_unresolved_type(arg, subst, seen))
              return true;
          }

          auto defs = def->look((elem / Ident)->location());

          if (defs.empty())
            return false;

          if (defs.size() > 1 && (it + 1) != type->end())
          {
            auto next_ident = (*(it + 1)) / Ident;
            auto picked = defs.front();

            for (auto& d : defs)
            {
              if (!d->look(next_ident->location()).empty())
              {
                picked = d;
                break;
              }
            }

            def = picked;
          }
          else
          {
            def = defs.front();
          }

          if (def == TypeParam)
          {
            if (!seen.insert(def).second)
              return true;

            auto find = subst.find(def);

            if (find == subst.end())
            {
              seen.erase(def);
              return true;
            }

            auto unresolved = has_unresolved_type(find->second, subst, seen);
            seen.erase(def);
            return unresolved;
          }
        }

        return false;
      }

      for (auto& child : *type)
      {
        if (has_unresolved_type(child, subst, seen))
          return true;
      }

      return false;
    }

    void emit_unresolved_type_error(const Node& blame, std::string_view context)
    {
      // During the two-stage worker run, defer error emission. A
      // formal that looks unresolved in stage 1 may bind in stage 2
      // (cross-function flow: callees emit constraints during their
      // stage 1, after this caller's stage 1 has already touched
      // r_type / Param emission). After worker.run() completes and
      // stage 2 has had its chance, defer is cleared and the
      // existing safety-net scans re-check for genuinely-unresolved
      // formals.
      if (defer_unresolved_errors)
        return;
      // Dedupe per (source location, context). Several reify_emitted_type
      // callers pass slightly different blame nodes (e.g. f / Ident vs
      // r->def / Ident) for the same source location, so use the blame's
      // source location as the dedup key, not the node identity.
      auto loc = blame->location();
      auto key = std::tuple<std::string_view, size_t, std::string>(
        loc.source ? loc.source->view() : std::string_view{},
        loc.pos,
        std::string(context));
      if (!reported_unresolved.insert(key).second)
        return;
      errors.push_back(err(
        blame,
        std::format("Could not resolve {} during monomorphization", context)));
    }

    bool defer_unresolved_errors{true};

    Node reify_emitted_type(
      const Node& source,
      const NodeMap<Node>& subst,
      const Node& blame,
      std::string_view context)
    {
      auto ir_type = reify_type(source, subst);

      if (contains_dyn(ir_type) && has_unresolved_type(source, subst))
        emit_unresolved_type_error(blame, context);

      return ir_type;
    }

    void run(Node& top_)
    {
      top = top_;
      builtin = top->look(Location("_builtin")).front();

      // Initialize the NodeWorker driver. The Work struct holds a back-
      // pointer to this Reifier so process() can dispatch to
      // reify_class / reify_function / reify_typealias.
      worker = std::make_unique<NodeWorker<ReifyWork>>(ReifyWork{this});

      // Create a call to main and reify it.
      auto main_module = top->front();
      assert(main_module == ClassDef);
      assert((main_module / TypeParams)->empty());

      // Check that main is defined under the main module and has no type
      // parameters.
      auto main_defs = main_module->look(Location("main"));
      if (main_defs.empty())
      {
        top << err(
          main_module,
          "No `main` function found under the main module: " +
            std::string((main_module / Ident)->location().view()));
        return;
      }

      auto main_def = main_defs.front();
      if ((main_def / TypeParams)->empty() == false)
      {
        top << err(
          main_def,
          "`main` function cannot have type parameters: " +
            std::string((main_def / Ident)->location().view()));
        return;
      }

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
      drain_worklist();
      resolve_shapes();
      prune_empty_shape_unions();
      process_pending_callbacks(false);
      drain_worklist();
      resolve_shapes();
      prune_empty_shape_unions();
      process_pending_callbacks(true);
      drain_worklist();

      // Phase 6 cross-function flow: by now every reification has
      // had stage 1 (constraint emission) AND stage 2 (re-solve)
      // run by the worker. However, ordering means some Resolutions
      // run their stage 2 BEFORE downstream reifications emit the
      // constraints that would bind their TypeVars. Run a final
      // pass: for every function reification, re-run stage 2 to
      // pick up newly-bound bindings. Loop to fixpoint since a
      // newly-bound formal in one function may reveal more bindings
      // for another via TypeVar union-find chains.
      bool fixpoint_changed = true;
      while (fixpoint_changed)
      {
        fixpoint_changed = false;
        for (auto& key : map_order)
        {
          for (auto& r : map[key])
          {
            if (!r.reification)
              continue;

            // Run function stage 2 OR class stage 2 (resolves
            // r.subst's TypeVar values via solver, propagates into
            // reified IR).
            if (key == Function)
            {
              NodeMap<Node> before_subst = r.subst;
              reify_function_stage2(r);
              bool changed_here = false;
              if (r.subst.size() != before_subst.size())
                changed_here = true;
              else
              {
                for (auto& [tp, val] : r.subst)
                {
                  auto bit = before_subst.find(tp);
                  if (bit == before_subst.end())
                  {
                    changed_here = true;
                    break;
                  }
                  if (!val && !bit->second)
                    continue;
                  if (!val || !bit->second)
                  {
                    changed_here = true;
                    break;
                  }
                  if (!val->equals(bit->second))
                  {
                    changed_here = true;
                    break;
                  }
                }
              }
              if (changed_here)
                fixpoint_changed = true;
            }
            else if (key == ClassDef)
            {
              // Class stage 2: re-resolve TypeVar values in r.subst,
              // then re-emit field types from the def using updated
              // subst. Without the field re-emission, captured vars
              // (e.g. lambda fields capturing a `var: T | none`) keep
              // the stage-1 Dyn placeholder and break typecheck.
              bool changed_here = false;
              for (auto& [tp, val] : r.subst)
              {
                if (!val || val != Type || val->empty() ||
                    val->front() != TypeVar)
                  continue;
                auto seed_id = typevar_store.intern(val->front());
                auto solved = typevar_store.solve(seed_id);
                if (!solved || (solved == Union && solved->empty()))
                  continue;
                bool has_tv = false;
                solved->traverse([&](const Node& n) {
                  if (n == TypeVar)
                    has_tv = true;
                  return !has_tv;
                });
                if (has_tv)
                  continue;
                Node inner = (solved == Type && !solved->empty()) ?
                  solved->front() : solved;
                r.subst[tp] = Type << clone(inner);
                changed_here = true;
              }
              if (changed_here)
                fixpoint_changed = true;

              // Re-emit field types when r.reification is a Class
              // node (Class << ClassId << Fields << Methods). Skip
              // shape reifications (resolve_shapes builds those).
              if (changed_here && r.reification && r.reification == Class)
              {
                Node fields_node = r.reification / Fields;
                if (fields_node)
                {
                  Node class_body = r.def / ClassBody;
                  if (class_body)
                  {
                    // Map field name → reified type from def.
                    std::map<Location, Node> def_field_types;
                    for (auto& member : *class_body)
                    {
                      if (member != FieldDef)
                        continue;
                      Node field_id = member / Ident;
                      Node field_type = member / Type;
                      if (!field_id || !field_type)
                        continue;
                      if (has_unresolved_type(field_type, r.subst))
                        continue;
                      auto reified = reify_type(field_type, r.subst);
                      if (reified && reified != Dyn)
                        def_field_types[field_id->location()] = reified;
                    }
                    for (auto& f : *fields_node)
                    {
                      if (f != Field)
                        continue;
                      Node fid = f / FieldId;
                      if (!fid)
                        continue;
                      auto it = def_field_types.find(fid->location());
                      if (it == def_field_types.end())
                        continue;
                      Node cur = f->back();
                      if (cur && !cur->equals(it->second))
                        f->replace(cur, clone(it->second));
                    }
                  }
                }
              }
            }
          }
        }
      }

      // Phase 6: post-fixpoint reification dedup.
      //
      // Two reifications that started with different initial substs
      // (one with a concrete type, one with a TypeVar inherited from
      // an enclosing seed) may have been allocated separate IDs by
      // find_or_push. After stage 2's fixpoint has bound the
      // TypeVars, those substs may now be equivalent — but the IDs
      // are baked in as ::0 and ::1.
      //
      // Walk every reification map slot. Within each (def, r_vec),
      // build a canonical map: the first entry whose subst is now
      // equal becomes the canonical id. Subsequent equivalent entries
      // have their reification cleared (they vanish from the IR) and
      // their ID is recorded for redirection. Then walk every reified
      // IR node, replacing non-canonical ids with their canonical
      // equivalent.
      std::map<Node, Node> id_redirect;  // non-canonical id → canonical id
      for (auto& [def, r_vec] : map)
      {
        if (r_vec.size() <= 1)
          continue;

        // Build canonical groups.
        std::vector<size_t> canonical_index(r_vec.size(), SIZE_MAX);
        for (size_t i = 0; i < r_vec.size(); i++)
        {
          if (canonical_index[i] != SIZE_MAX)
            continue;
          canonical_index[i] = i;
          for (size_t j = i + 1; j < r_vec.size(); j++)
          {
            if (canonical_index[j] != SIZE_MAX)
              continue;
            if (subst_equal(r_vec[i].subst, r_vec[j].subst))
              canonical_index[j] = i;
          }
        }
        // Record redirects and clear non-canonical reifications.
        for (size_t i = 0; i < r_vec.size(); i++)
        {
          if (canonical_index[i] == i)
            continue;
          auto& canonical_r = r_vec[canonical_index[i]];
          if (!canonical_r.id || !r_vec[i].id)
            continue;
          if (canonical_r.id->equals(r_vec[i].id))
            continue;
          id_redirect[r_vec[i].id] = canonical_r.id;
          // Drop the duplicate's IR — it's a stale copy. The
          // canonical entry's IR is what survives.
          r_vec[i].reification = {};
        }
      }

      // Walk every reified IR node, replacing non-canonical ids with
      // their canonical equivalent. Compare by content (location
      // string) since the IR nodes are clones.
      auto canonical_id = [&](const Node& n) -> Node {
        for (auto& [src, dst] : id_redirect)
        {
          if (n->type() == src->type() && n->location() == src->location())
            return dst;
        }
        return {};
      };

      std::function<void(const Node&)> redirect_ids;
      redirect_ids = [&](const Node& node) {
        if (!node)
          return;
        for (size_t i = 0; i < node->size(); i++)
        {
          Node child = node->at(i);
          if (!child)
            continue;
          if (child->type().in({ClassId, FunctionId, TypeId}))
          {
            auto repl = canonical_id(child);
            if (repl)
              node->replace_at(i, clone(repl));
            continue;
          }
          redirect_ids(child);
        }
      };

      if (!id_redirect.empty())
      {
        for (auto& [def, r_vec] : map)
        {
          for (auto& r : r_vec)
          {
            if (r.reification)
              redirect_ids(r.reification);
          }
        }
      }

      // Phase 6: substitute resolved_name TypeVar leaves with their
      // solved bindings. resolved_name was built at find_or_push
      // time using the caller's then-current subst; if the caller
      // had a TypeVar seed, resolved_name kept that TypeVar in its
      // TypeArgs. resolve_shapes uses resolved_name for Subtype
      // matching; an unbound TypeVar leaf there causes legitimate
      // shape implementors to be excluded from union members.
      std::function<void(const Node&)> substitute_resolved_typevars;
      substitute_resolved_typevars = [&](const Node& node) {
        if (!node)
          return;
        for (size_t i = 0; i < node->size(); i++)
        {
          Node child = node->at(i);
          if (!child)
            continue;
          if (child == TypeVar)
          {
            auto seed_id = typevar_store.intern(child);
            auto solved = typevar_store.solve(seed_id);
            if (solved && !(solved == Union && solved->empty()))
            {
              bool has_tv = false;
              solved->traverse([&](const Node& n) {
                if (n == TypeVar)
                  has_tv = true;
                return !has_tv;
              });
              if (!has_tv)
              {
                Node inner = (solved == Type && !solved->empty()) ?
                  solved->front() : solved;
                node->replace_at(i, clone(inner));
                continue;
              }
            }
          }
          substitute_resolved_typevars(child);
        }
      };

      for (auto& [def, r_vec] : map)
      {
        for (auto& r : r_vec)
        {
          if (r.resolved_name)
            substitute_resolved_typevars(r.resolved_name);
        }
      }

      // Re-run resolve_shapes once more after dedup + resolved_name
      // substitution so shape unions reflect the canonicalized class
      // ids and the post-fixpoint resolved_names. Clear the cache
      // because earlier runs computed Subtype results for stale
      // resolved_names (with TypeVar leaves) that may have been
      // false negatives.
      shape_subtype_cache.clear();
      resolve_shapes();

      // Re-enable error emission for the existing safety-net scans
      // below. Any TypeVar that survives is a genuine unbound formal
      // that the user must resolve.
      defer_unresolved_errors = false;

      std::vector<Reification*> reified_functions;

      for (auto& key : map_order)
      {
        if (key != Function)
          continue;

        for (auto& r : map[key])
        {
          if (r.reification)
            reified_functions.push_back(&r);
        }
      }

      // Second pass: refine unresolved parameter and return types from the
      // already-reified bodies now that all callees and methods are available.
      bool changed;
      do
      {
        changed = false;

        for (auto r : reified_functions)
        {
          auto func = r->reification;
          auto labels = func / Labels;

          // Build a predecessor map so we can compute per-label entry
          // environments with typetest-induced narrowing on Cond edges.
          auto preds_map = build_label_pred_map(labels);

          // Per-label exit environments. We iterate to fixpoint so that
          // back-edges (e.g. from while-loops) propagate.
          std::map<Location, LocalEnv> label_exits;

          bool inner_changed = true;
          while (inner_changed)
          {
            inner_changed = false;

            for (size_t li = 0; li < labels->size(); ++li)
            {
              auto lbl = labels->at(li);
              Location lbl_loc = (lbl / LabelId)->location();

              // Compute the entry env for this label.
              LocalEnv entry;

              if (li == 0)
              {
                // Entry label starts with parameter types.
                for (auto& p : *(func / Params))
                  entry[(p / LocalId)->location()] = clone(p / Type);
              }
              else
              {
                auto pit = preds_map.find(lbl_loc);
                if (pit != preds_map.end())
                {
                  for (auto& edge : pit->second)
                  {
                    auto exit_it = label_exits.find(edge.pred_loc);
                    if (exit_it == label_exits.end())
                      continue;

                    auto narrowed =
                      apply_edge_narrowing(exit_it->second, edge.narrowing);
                    merge_env_into(entry, narrowed);
                  }
                }
              }

              // Reset per-label state. lookup_info is set inside the body
              // and consumed in the same label, so clear per label.
              local_types = std::move(entry);
              lookup_info.clear();

              for (auto& stmt : *(lbl / Body))
              {
                if (stmt->in({Const, Convert}))
                {
                  local_types[(stmt / LocalId)->location()] =
                    clone(stmt / Type);
                }
                else if (stmt == ConstStr)
                {
                  local_types[(stmt / LocalId)->location()] = Array
                    << clone(U8);
                }
                else if (stmt->in({Copy, Move}))
                {
                  auto src_it = local_types.find((stmt / Rhs)->location());
                  if (src_it != local_types.end())
                  {
                    auto dst_loc = (stmt / LocalId)->location();
                    auto dst_it = local_types.find(dst_loc);

                    local_types[dst_loc] = (dst_it == local_types.end()) ?
                      clone(src_it->second) :
                      merge_refined_type(dst_it->second, src_it->second);
                  }
                }
                else if (stmt->in({New, Stack}))
                {
                  local_types[(stmt / LocalId)->location()] =
                    clone(stmt / ClassId);
                }
                else if (stmt == FieldRef)
                {
                  auto obj_loc = (stmt / Arg / Rhs)->location();
                  auto obj_it = local_types.find(obj_loc);
                  if (
                    obj_it != local_types.end() && (obj_it->second == ClassId))
                  {
                    auto ft = find_field_type(obj_it->second, stmt / FieldId);
                    if (ft)
                      local_types[(stmt / LocalId)->location()] = Ref << ft;
                  }
                }
                else if (stmt == Lookup)
                {
                  auto mid = (stmt / MethodId)->location().view();
                  lookup_info[(stmt / LocalId)->location()] = {
                    std::string(mid), (stmt / Rhs)->location()};
                }
                else if (stmt == Call)
                {
                  if (
                    auto* target = find_function_reification(stmt / FunctionId))
                    changed |=
                      refine_function_params(*target, stmt->at(2), false);

                  auto ret = find_func_return_type(stmt / FunctionId);
                  if (ret)
                    local_types[(stmt / LocalId)->location()] = ret;
                }
                else if (stmt->in({CallDyn, TryCallDyn}))
                {
                  auto src_loc = (stmt / Rhs)->location();
                  auto li = lookup_info.find(src_loc);
                  if (li != lookup_info.end())
                  {
                    auto recv_it = local_types.find(li->second.recv_loc);
                    if (recv_it != local_types.end())
                    {
                      auto targets = find_method_targets(
                        recv_it->second,
                        li->second.method_id,
                        stmt->at(2),
                        false);

                      if (
                        targets.empty() &&
                        receiver_is_param(func, li->second.recv_loc))
                      {
                        auto fallback_targets = find_method_targets(
                          Dyn, li->second.method_id, stmt->at(2), false);

                        if (refine_receiver_type(
                              func,
                              li->second.recv_loc,
                              recv_it->second,
                              fallback_targets))
                        {
                          changed = true;
                          targets = std::move(fallback_targets);
                        }
                      }

                      bool unresolved_receiver =
                        contains_dyn(recv_it->second) ||
                        contains_typeid(recv_it->second);

                      bool skip_param_refinement =
                        unresolved_receiver && (targets.size() > 1);

                      if (!skip_param_refinement)
                      {
                        for (auto* target : targets)
                        {
                          if (target)
                            changed |= refine_function_params(
                              *target, stmt->at(2), false);
                        }
                      }

                      auto ret = find_method_return_type(targets);

                      if (!ret && recv_it->second == Ref)
                      {
                        auto& mid = li->second.method_id;
                        if (mid.starts_with("*::"))
                          ret = clone(recv_it->second->front());
                      }

                      if (ret)
                        local_types[(stmt / LocalId)->location()] = ret;
                    }
                  }
                }
                else if (stmt == Load)
                {
                  auto src_it = local_types.find((stmt / Rhs)->location());
                  if (src_it != local_types.end() && (src_it->second == Ref))
                    local_types[(stmt / LocalId)->location()] =
                      clone(src_it->second->front());
                }
                else if (stmt == WhenDyn)
                {
                  auto cown_type = stmt / Cown;
                  auto li = lookup_info.find((stmt / Rhs)->location());

                  if (li != lookup_info.end())
                  {
                    auto recv_it = local_types.find(li->second.recv_loc);

                    if (recv_it != local_types.end())
                    {
                      auto targets = find_method_targets(
                        recv_it->second,
                        li->second.method_id,
                        stmt->at(2),
                        true);

                      if (
                        targets.empty() &&
                        receiver_is_param(func, li->second.recv_loc))
                      {
                        auto fallback_targets = find_method_targets(
                          Dyn, li->second.method_id, stmt->at(2), true);

                        if (refine_receiver_type(
                              func,
                              li->second.recv_loc,
                              recv_it->second,
                              fallback_targets))
                        {
                          changed = true;
                          targets = std::move(fallback_targets);
                        }
                      }

                      bool unresolved_receiver =
                        contains_dyn(recv_it->second) ||
                        contains_typeid(recv_it->second);

                      bool skip_param_refinement =
                        unresolved_receiver && (targets.size() > 1);

                      if (!skip_param_refinement)
                      {
                        for (auto* target : targets)
                        {
                          if (target)
                            changed |= refine_function_params(
                              *target, stmt->at(2), true);
                        }
                      }

                      // TypeVar-returning behaviors may have been reified with
                      // a provisional return (for example just nomatch) before
                      // the deferred return-type pass converges. Refresh the
                      // result cown type when the reified return differs from
                      // the current cown type.
                      {
                        auto ret = find_method_return_type(targets);

                        if (ret && (ret != Dyn) && (ret != TypeVar))
                        {
                          Node new_cown = Cown << clone(ret);

                          if (!cown_type->equals(new_cown))
                          {
                            stmt->replace(cown_type, new_cown);
                            changed = true;
                          }

                          cown_type = stmt / Cown;
                        }
                      }
                    }
                  }

                  local_types[(stmt / LocalId)->location()] = clone(cown_type);
                }
                else if (stmt == When)
                {
                  auto cown_type = stmt / Cown;

                  if (
                    auto* target = find_function_reification(stmt / FunctionId))
                  {
                    changed |=
                      refine_function_params(*target, stmt->at(2), true);

                    bool needs_refresh = (cown_type == Cown) &&
                      ((cown_type->front() == Dyn) ||
                       (cown_type->front() == TypeVar) ||
                       ((target->def / Type)->front() == TypeVar));

                    if (needs_refresh)
                    {
                      Node ret = target->reification / Type;

                      if (ret && (ret != Dyn) && (ret != TypeVar))
                      {
                        Node new_cown = Cown << clone(ret);

                        if (!cown_type->equals(new_cown))
                        {
                          stmt->replace(cown_type, new_cown);
                          changed = true;
                        }

                        cown_type = stmt / Cown;
                      }
                    }
                  }

                  local_types[(stmt / LocalId)->location()] = clone(cown_type);
                }
              }

              // Save exit env. If the env changed, run another inner
              // iteration to propagate to successors.
              auto& saved = label_exits[lbl_loc];
              if (!envs_equal(saved, local_types))
              {
                saved = local_types;
                inner_changed = true;
              }
            }
          }

          auto current_ret = func / Type;
          if (
            ((r->def / Type)->front() == TypeVar) ||
            is_nomatch_ir(current_ret) ||
            (contains_dyn(current_ret) &&
             contains_typevar_leaf(r->def / Type)))
          {
            // Now try to infer the return type from Return locals.
            // Collect all distinct return types to build a union if needed.
            // Use each label's own exit env (with typetest narrowing) so the
            // inferred return type is precise.
            Nodes ret_types;

            for (auto& lbl : *labels)
            {
              auto term = lbl / Return;
              if (term != Return)
                continue;

              auto ret_loc = (term / LocalId)->location();
              Location lbl_loc = (lbl / LabelId)->location();

              auto eit = label_exits.find(lbl_loc);
              if (eit == label_exits.end())
                continue;

              auto it = eit->second.find(ret_loc);
              if (it == eit->second.end())
                continue;

              bool dup = false;

              for (auto& existing : ret_types)
              {
                if (existing->equals(it->second))
                {
                  dup = true;
                  break;
                }
              }

              if (!dup)
                ret_types.push_back(clone(it->second));
            }

            Node new_ret;

            if (ret_types.size() == 1)
              new_ret = ret_types.front();
            else if (ret_types.size() > 1)
            {
              Node union_node = Union;

              for (auto& rt : ret_types)
                union_node << clone(rt);

              new_ret = union_node;
            }

            if (new_ret && new_ret->type() != Dyn)
            {
              auto old_type = func / Type;
              if (!old_type->equals(new_ret))
              {
                func->replace(old_type, new_ret);
                changed = true;
              }
            }
          }

          // VarDef refinement: VarDef types are emitted in the first pass
          // body walk using local_types observed at that time. If a var's
          // type couldn't be tracked then (e.g., because a CallDyn's
          // receiver was a not-yet-reified lambda), the first-pass fallback
          // emitted Dyn. Now that the second pass has refined local_types,
          // update VarDef types from the aggregated label exits.
          //
          // This is the Dyn-rule fix at site VarDef (line ~3558): when the
          // second pass produces a concrete type for a var that the first
          // pass left as Dyn, replace the Dyn with the concrete type.
          {
            std::map<Location, Node> aggregated;

            for (auto& [lbl_loc, env] : label_exits)
            {
              for (auto& [var_loc, type] : env)
              {
                auto it = aggregated.find(var_loc);
                if (it == aggregated.end())
                  aggregated[var_loc] = clone(type);
                else
                  aggregated[var_loc] =
                    merge_refined_type(it->second, type);
              }
            }

            auto vars_node = func / Vars;

            for (auto& vd : *vars_node)
            {
              auto loc = (vd / LocalId)->location();
              auto it = aggregated.find(loc);
              if (it == aggregated.end())
                continue;
              if (it->second == Dyn)
                continue;

              auto old_type = vd / Type;
              // First-pass concrete types (not placeholders) come from
              // a pinned TypeAssertion (the user's declared type). They
              // are the source of truth — second-pass aggregation may
              // narrow (e.g. observed only the init value `none`) or
              // widen (gather `nomatch` from match arms), but the
              // declared type wins. Only refine when first pass left a
              // placeholder (TypeVar / Dyn).
              if (old_type != TypeVar && old_type != Dyn)
                continue;

              if (old_type->equals(it->second))
                continue;

              vd->replace(old_type, clone(it->second));
              changed = true;
            }
          }
        }

      } while (changed);

      for (auto r : deferred_typevar)
      {
        if (r->reification && ((r->reification / Type) == TypeVar))
        {
          emit_unresolved_type_error(r->def / Ident, "return type");
          // Convert to Dyn so wfType validates; the error already fails
          // the compile.
          auto t = r->reification / Type;
          r->reification->replace(t, clone(Dyn));
        }
      }

      for (auto r : reified_functions)
      {
        auto params = r->reification / Params;
        auto def_params = r->def / Params;

        for (size_t i = 0; i < params->size(); i++)
        {
          if (
            contains_dyn(params->at(i) / Type) &&
            has_unresolved_type((def_params->at(i) / Type), r->subst))
          {
            emit_unresolved_type_error(
              def_params->at(i) / Ident, "parameter type");
          }
        }

        // Return-type leak check. The stage-2 re-derivation at line ~1031
        // handles cases where the source return type contained a TypeVar
        // that bound from label_exits. If, after stage 2 / fixpoint, the
        // IR return type still contains Dyn AND the source still has an
        // unresolved type (TypeVar leaf or unbound TypeParam), the leak
        // is genuinely irreparable — emit an error so the compile fails
        // rather than silently producing dyn-typed downstream IR.
        if (
          contains_dyn(r->reification / Type) &&
          has_unresolved_type(r->def / Type, r->subst))
        {
          emit_unresolved_type_error(r->def / Ident, "return type");
        }

        // Safety net for the TypeVar VarDef intermediate marker
        // (line ~3672). The second-pass refinement at line ~735 should
        // have replaced TypeVar with the concrete aggregate type. If
        // any TypeVar persists here, the var was genuinely untrackable —
        // emit an error and convert to Dyn so wfType validates.
        auto vars_node = r->reification / Vars;
        for (auto& vd : *vars_node)
        {
          auto vt = vd / Type;
          if (vt == TypeVar)
          {
            emit_unresolved_type_error(r->def / Ident, "var type");
            vd->replace(vt, clone(Dyn));
          }
        }

        // Safety net for the TypeVar WhenDyn cown intermediate marker
        // (line ~4839). Walk the function body, find any WhenDyn whose
        // cown content is still TypeVar, emit an error and convert to
        // Dyn for wfType validity. The second-pass refinement at line
        // ~640 should have replaced TypeVar by now.
        auto labels = r->reification / Labels;
        for (auto& lbl : *labels)
        {
          auto body = lbl / Body;
          for (auto& stmt : *body)
          {
            if (stmt != WhenDyn)
              continue;
            auto cown = stmt / Cown;
            if ((cown == Cown) && (cown->front() == TypeVar))
            {
              emit_unresolved_type_error(
                r->def / Ident, "when block return type");
              auto tv = cown->front();
              cown->replace(tv, clone(Dyn));
            }
          }
        }
      }

      resolve_shapes();

      // Phase 6 [P9] IR-boundary scan: after all reifications complete
      // and the existing safety nets have run, walk every reification
      // for free TypeVar leaves. Per the design plan, the constraint
      // solver should have bound every formal that's used in the
      // emitted IR; any surviving TypeVar is a hard error citing the
      // TypeVar's source Location. This catches:
      //  - Generic instantiations whose unbound formal couldn't be
      //    inferred (ought to be the user's responsibility — explicit
      //    type argument).
      //  - apply_subst paths that returned clone(type_node) for a
      //    free TypeVar without a downstream binding.
      //
      // Per AGENTS.md, Dyn is the IR encoding of `any` and must NOT
      // be used as a fallback for unresolved types. We emit an error
      // and let the rebuild-top loop below carry on; if errors are
      // already present, ninja will surface them.
      std::set<std::string> typevar_seen;
      for (auto& key : map_order)
      {
        for (auto& r : map[key])
        {
          if (!r.reification)
            continue;
          r.reification->traverse([&](const Node& n) {
            if (n != TypeVar)
              return true;
            auto loc_view = n->location().view();
            auto key_str = std::string(loc_view);
            if (!typevar_seen.insert(key_str).second)
              return true; // dedup per Location
            Node blame = r.def;
            if (r.def && r.def->size() > 0)
            {
              Node ident = r.def / Ident;
              if (ident)
                blame = ident;
            }
            errors.push_back(err(
              blame,
              std::format(
                "Reified IR retains a free TypeVar (location '{}') for {}. "
                "The type parameter could not be inferred — provide an "
                "explicit type argument at the call site.",
                loc_view,
                r.def == Function ? "this function" :
                  (r.def == ClassDef ? "this class" : "this reification"))));
            return true;
          });
        }
      }

      // Remove existing contents.
      top->erase(top->begin(), top->end());

      // Add an entry point for main.
      top
        << (Func << (FunctionId ^ "@main") << Params << None << Vars
                 << (Labels
                     << (Label << (LabelId ^ "start") << (Body << main_call)
                               << (Return << (LocalId ^ id)))));

      // Add reified classes, type aliases, and functions.
      // Emit non-Primitive entries first so that Type entries (which
      // define TypeId resolutions) are in top before we check Primitives.
      for (auto& key : map_order)
        for (auto& r : map[key])
          if (r.reification && r.reification != Primitive)
            top << r.reification;

      // Emit Primitives, deduplicating wrappers whose inner types are
      // invariantly equivalent after alias/shape resolution. Rebuild the
      // wfIR symbol table after populating top so IRSubtype can resolve
      // TypeId through Top::look(), then use it for union set equality and
      // wrapper invariance.
      {
        WFContext wf_ctx(wfIR);
        wfIR.build_st(top);
        std::vector<std::pair<Token, Node>> emitted;

        for (auto& key : map_order)
        {
          for (auto& r : map[key])
          {
            if (!r.reification || r.reification != Primitive)
              continue;

            auto ptype = r.reification / Type;

            if (ptype->in({Array, Ref, Cown}))
            {
              auto wrapper = ptype->type();
              auto inner = ptype->front();
              bool dup = false;

              for (auto& [ew, ei] : emitted)
              {
                if (ew == wrapper && vbcc::IRSubtype.invariant(top, inner, ei))
                {
                  dup = true;
                  break;
                }
              }

              if (dup)
                continue;

              emitted.emplace_back(wrapper, inner);
            }

            top << r.reification;
          }
        }
      }

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
      // Source AST def (Function / ClassDef / TypeAlias).
      Node def;
      // Substitution for this reification's TypeParams.
      NodeMap<Node> subst;
      // Canonical id Node. Set once at construction in find_or_push.
      // INVARIANT: this is the canonical Node used as a worker key
      // (Phase 3+ NodeWorker) and as the IR ClassId / FunctionId
      // child of r.reification (e.g., line ~3542 `Func << r.id`).
      // It MUST NOT be cloned by find_or_push's return path —
      // find_or_push returns clone(id) for AST-embedding callers
      // who need their own parented copy. Direct access via
      // r.id from a Reification* always yields the canonical.
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
      bool all_receivers; // true = all classes/primitives may receive the call
      Nodes
        receivers; // concrete possible receivers when all_receivers is false
    };

    struct PendingCallback
    {
      Node site;
      Node type;
      bool required;
    };

    // Forward struct for the NodeWorker driver. ReifyWork is the Work
    // type for NodeWorker<ReifyWork>; per-reification state is keyed by
    // Reification.id and carries a back-pointer to the Reification.
    // Implemented after Reifier so its methods can call back into
    // Reifier-side dispatch (reify_class / reify_function / reify_typealias).
    struct ReifyWork
    {
      Reifier* outer{nullptr};

      struct State : NodeWorkerState
      {
        Reification* reif{nullptr};

        // Two-stage reify pipeline:
        //   Init: nothing done yet.
        //   NeedSolve: stage 1 done. block_on_all (via the side
        //              direct_deps_by_parent map). When unblocked,
        //              stage 2 runs.
        //   Done: stage 2 done.
        // The stage transition lets sum's solve run AFTER reduce's
        // body emissions land in the global typevar_store.
        enum class Stage : uint8_t { Init, NeedSolve, Done };
        Stage stage{Stage::Init};
      };

      void seed(const Node& /*id*/, State& /*s*/) {}

      bool process(const Node& id, NodeWorker<ReifyWork>& worker);
    };

    Node top;
    Node builtin;
    NodeMap<std::deque<Reification>> map;
    std::vector<Node> map_order;
    // NodeWorker driver. Each Reification is a work item keyed by
    // Reification.id (the canonical id Node from make_id). Replaces the
    // legacy std::vector<Reification*> worklist + drain_worklist loop;
    // gives explicit dependency tracking via block_on for mutual /
    // forward-reference scenarios. State holds a back-pointer to the
    // Reification so process() can dispatch to reify_class /
    // reify_function / reify_typealias.
    std::unique_ptr<NodeWorker<ReifyWork>> worker;
    // Reifications that had a TypeVar return type in their source def.
    // Populated by ReifyWork::process when reify_function completes.
    // Used by the post-worklist refinement loop and unresolved-return
    // error scan.
    std::vector<Reification*> deferred_typevar;
    // Phase 6 cross-function flow: store each function reification's
    // formal_typevars (TypeParam → α_id) and seed-TV map so the
    // stage-2 (post-block) re-solve can re-query the solver after
    // callees have emitted their constraints.
    std::unordered_map<Reification*, std::vector<std::pair<Node, uint32_t>>>
      reify_formal_alphas;
    // Map from seed TypeVar Location → Reification* of the function
    // that seeded it, so during stage 2 we can solve the right α
    // even for transitively-inherited seeds.
    std::map<Location, std::pair<Reification*, Node>> seed_owner_map;
    // Stack of active reification ids in the current process() chain;
    // find_or_push consults the top to record direct_deps. Holds Node
    // ids (stable identity) instead of State* — State pointers may be
    // invalidated by NodeMap rehashing when new reifications register.
    std::vector<Node> reify_id_stack;

    // Side-map: parent reification id → vector of direct dep ids
    // collected during stage 1. process() consumes these to call
    // block_on_all. Side-mapped because worker.state() uses pointer
    // equality on Node — and record_dep is called with `clone(id)`
    // which doesn't match the original key.
    std::map<Node, std::vector<Node>> direct_deps_by_parent;
    std::map<Location, Node> libs;
    NodeMap<Node> init_sources;
    std::set<Node> processed_initfini;
    Nodes errors;
    // Dedupe set for "Type parameter X cannot be inferred" errors:
    // a given unbound formal is reported at most once across the
    // whole pass, even though many navigation paths may try to
    // resolve it. Multiple downstream errors all point at the same
    // root cause; one diagnostic at the use site is sufficient.
    std::set<const NodeDef*> reported_unbound_formal;
    std::set<std::tuple<std::string_view, size_t, std::string>>
      reported_unresolved;
    // TypeParams currently being resolved (used to detect
    // self-referential substitutions, which would otherwise infinite-
    // loop in get_reification → reify_type → reify_typename →
    // get_reification).
    std::set<const NodeDef*> resolving_typeparams;
    std::vector<MethodInvocation> method_invocations;
    std::vector<PendingCallback> pending_callbacks;
    std::map<std::string, std::vector<std::vector<Node>>> method_index;
    std::map<std::pair<const NodeDef*, const NodeDef*>, bool>
      shape_subtype_cache;

    // Per-function local type map: LocalId location -> reified type.
    // Populated during reify_function, used by reify_lookup.
    std::map<Location, Node> local_types;

    // Per-function lookup info: maps Lookup dst location to
    // {MethodId string, receiver location}. Used to resolve CallDyn and
    // When return types.
    struct LookupInfo
    {
      std::string method_id;
      Location recv_loc;
    };
    // Register a Reification with the NodeWorker. Sets the back-pointer
    // so process() can dispatch on def kind. Idempotent: calling add()
    // again on an already-seeded id is a no-op (NodeWorker checks).
    void register_with_worker(Reification* r)
    {
      worker->add(r->id);
      worker->state(r->id).reif = r;
    }

    // Phase 6 cross-function flow: record `id` as a direct dependency
    // of the topmost active reification. Called from find_or_push for
    // every id returned (newly created or existing). Stage 1 of the
    // active reify uses these to block_on_all in process(). Uses a
    // side-map (not worker.state) because find_or_push returns
    // clone(id) which has different pointer identity from the original
    // worker.state key — std::map<Node,V> uses pointer comparison.
    void record_dep(const Node& id)
    {
      if (reify_id_stack.empty())
        return;
      Node parent_id = reify_id_stack.back();
      Node candidate = id;
      // Don't depend on ourselves (compare by content via equals).
      if (parent_id->equals(candidate))
        return;
      direct_deps_by_parent[parent_id].push_back(id);
    }

    std::map<Location, LookupInfo> lookup_info;

    // Phase 3b.5 (b): concrete-receiver taint tracking. A local is
    // Locals whose type is pinned by a concrete TypeAssertion (e.g.,
    // `var best: i32 | none = none`). Subsequent Copy/Move into the
    // local must NOT overwrite local_types[loc] — the user's declared
    // type is the source of truth. Without this, captured-ref fields
    // pick up the init value's narrower type instead of the declared
    // union type, causing typecheck failures at the lambda New site.
    // Cleared per reify_function (same lifetime as local_types).
    std::set<Location> pinned_locals;

    // typevar_store is the SAME singleton used by infer (constraints
    // emitted from subtype calls accumulate here) and by reify
    // (cross-reify gather and per-Reification body walks). Identities
    // are global Locations (TypeVar Locations and TypeParam Ident
    // Locations), so accumulation across passes and functions is
    // safe and monotone.
    TypeVarStore& typevar_store = TypeVarStore::global();

    // Phase B2: pre-pass emission of arg-vs-formal Subtype constraints
    // at every Call/CallDyn site in `func_def`'s body, using SOURCE
    // types (taken from the AST's param/return/Const annotations) and
    // the caller's r.subst applied (which carries α_k seeds for any
    // unbound formals). This is the reify-side counterpart of the
    // infer-side call-arg emission: at infer time we couldn't fire on
    // generic callers (e.g. each_min[T,U]) because T was parametric;
    // here, T has already been substituted to a concrete class type,
    // so navigating the receiver and resolving the method is
    // possible. Subtype runs in Mode::Emit, so any TypeVar (or
    // TypeName-resolving-to-TypeParam) hit during decomposition
    // emits add_lower / add_upper / unify into the global store.
    void emit_source_call_constraints(
      const Node& func_def, const NodeMap<Node>& subst)
    {
      // source_types tracks each local's *source* type (i.e., the
      // type as written in the AST, with caller TypeArg references
      // unsubstituted). At emission points we apply caller's subst.
      std::map<Location, Node> source_types;

      // Initialize from params: each param has a Type child.
      for (auto& p : *(func_def / Params))
      {
        Node t = p / Type;
        if (t == Type)
          source_types[(p / Ident)->location()] = clone(t);
      }

      auto emit_subtype = [&](const Node& arg_type, const Node& formal_type) {
        if (!arg_type || !formal_type)
          return;
        SequentCtx emit_ctx{top, {}, {}};
        emit_ctx.constraint_store = &typevar_store;
        emit_ctx.mode = SequentCtx::Mode::Emit;
        (void)Subtype(emit_ctx, arg_type, formal_type);
      };

      // For Call: navigate FuncName → Function def, build callee_subst
      // from FuncName TypeArgs, then for each (param, arg) pair emit
      // Subtype(arg_type_post_subst, formal_type_post_subst).
      auto emit_for_call = [&](const Node& call) {
        Node fname = call / FuncName;
        if (!fname || fname == FuncName)
        {
          auto def = find_def(top, fname);
          if (!def || def != Function)
            return;
          auto fparams = def / Params;
          auto fargs = call / Args;
          if (fparams->size() != fargs->size())
            return;
          // Build callee_subst from FuncName's NameElement TypeArgs.
          NodeMap<Node> callee_subst = build_subst_from_typename(top, fname);
          for (size_t i = 0; i < fparams->size(); i++)
          {
            auto pt = fparams->at(i) / Type;
            auto formal = apply_subst(top, pt, callee_subst);
            // Apply caller's subst to surface α_k identities in formal
            // (in case callee inherits caller TypeParams via TypeArgs).
            formal = apply_subst(top, formal, subst);
            auto arg_loc = (fargs->at(i) / Rhs)->location();
            auto it = source_types.find(arg_loc);
            if (it == source_types.end())
              continue;
            auto arg_type = apply_subst(top, it->second, subst);
            emit_subtype(arg_type, formal);
          }
          // Track the call result's source type (return type post
          // subst) so subsequent uses of the result drive emission.
          Node ret_type = def / Type;
          if (ret_type)
          {
            auto resolved_ret = apply_subst(top, ret_type, callee_subst);
            resolved_ret = apply_subst(top, resolved_ret, subst);
            if (resolved_ret)
              source_types[(call / LocalId)->location()] = resolved_ret;
          }
        }
      };

      // For CallDyn: previous Lookup gives us the receiver's local id
      // and the method ident. Apply caller subst to the receiver's
      // source type, find its class def, find the method by name +
      // arity, then per-(param, arg) emit Subtype.
      std::map<Location, std::pair<Location, Node>> lookup_method_info;

      auto emit_for_calldyn = [&](const Node& call) {
        auto src_loc = (call / Rhs)->location();
        auto li = lookup_method_info.find(src_loc);
        if (li == lookup_method_info.end())
          return;
        auto recv_loc = li->second.first;
        Node lookup_node = li->second.second;
        auto rit = source_types.find(recv_loc);
        if (rit == source_types.end())
          return;
        auto recv_resolved = apply_subst(top, rit->second, subst);
        if (!recv_resolved || recv_resolved != Type || recv_resolved->empty())
          return;
        auto recv_inner = recv_resolved->front();
        if (recv_inner != TypeName)
          return;
        auto cls_def = find_def(top, recv_inner);
        if (!cls_def || cls_def != ClassDef)
          return;

        auto method_name = (lookup_node / Ident)->location();
        auto method_hand = (lookup_node / Lhs)->type();
        auto fargs = call / Args;
        size_t arity = fargs->size();

        Node method_def;
        for (auto& member : *(cls_def / ClassBody))
        {
          if (member != Function)
            continue;
          if ((member / Ident)->location() != method_name)
            continue;
          if ((member / Lhs)->type() != method_hand &&
              !((member / Lhs) == Once && method_hand == Rhs))
            continue;
          if ((member / Params)->size() != arity)
            continue;
          method_def = member;
          break;
        }
        if (!method_def)
          return;

        // Class-level subst from the receiver's TypeName.
        NodeMap<Node> cls_subst = build_subst_from_typename(top, recv_inner);

        auto fparams = method_def / Params;
        for (size_t i = 0; i < fparams->size(); i++)
        {
          auto pt = fparams->at(i) / Type;
          auto formal = apply_subst(top, pt, cls_subst);
          formal = apply_subst(top, formal, subst);
          auto arg_loc = (fargs->at(i) / Rhs)->location();
          auto it = source_types.find(arg_loc);
          if (it == source_types.end())
            continue;
          auto arg_type = apply_subst(top, it->second, subst);
          emit_subtype(arg_type, formal);
        }

        // Track the calldyn result's source type so subsequent uses
        // (Copy into a U-typed local, etc.) can drive emission.
        Node ret_type = method_def / Type;
        if (ret_type)
        {
          auto resolved_ret = apply_subst(top, ret_type, cls_subst);
          resolved_ret = apply_subst(top, resolved_ret, subst);
          if (resolved_ret)
            source_types[(call / LocalId)->location()] = resolved_ret;
        }
      };

      // Walk source body: track source_types on definition statements,
      // emit constraints at Call/CallDyn statements.
      for (auto& l : *(func_def / Labels))
      {
        Node body = l / Body;
        for (auto& n : *body)
        {
          if (!n)
            continue;

          if (n->in({Const, Convert, New, Stack}))
          {
            Node t = n / Type;
            if (t == Type)
              source_types[(n / LocalId)->location()] = clone(t);
          }
          else if (n == TypeAssertion)
          {
            Node t = n / Type;
            if (t == Type)
              source_types[(n / LocalId)->location()] = clone(t);
          }
          else if (n->in({Copy, Move}))
          {
            auto src_loc = (n / Rhs)->location();
            auto dst_loc = (n / LocalId)->location();
            auto src_it = source_types.find(src_loc);
            auto dst_it = source_types.find(dst_loc);

            // Variance table: writing a value of type T into a local
            // typed α emits add_lower(α, T) (equivalently Subtype(T, α)).
            // Run subtype in Mode::Emit so the Query-pass-first guard
            // skips emission when both sides are concrete and prove
            // structurally without α involvement.
            if (
              src_it != source_types.end() && dst_it != source_types.end())
            {
              auto src_type = apply_subst(top, src_it->second, subst);
              auto dst_type = apply_subst(top, dst_it->second, subst);
              emit_subtype(src_type, dst_type);
            }

            // Propagate src's source_type to dst (consistent with the
            // body's actual type flow). The TypeAssertion-set source_type
            // gets overwritten by subsequent Copy/Move from a concrete
            // src — that's correct because the assignment's RHS is what
            // dst HOLDS at runtime; its declared type is just an upper
            // bound the constraint above already enforces.
            if (src_it != source_types.end())
              source_types[dst_loc] = clone(src_it->second);
          }
          else if (n == Lookup)
          {
            auto recv_loc = (n / Rhs)->location();
            lookup_method_info[(n / LocalId)->location()] = {recv_loc, n};
          }
          else if (n == Call)
          {
            emit_for_call(n);
          }
          else if (n->in({CallDyn, TryCallDyn}))
          {
            emit_for_calldyn(n);
          }
        }
      }

      // Phase 4 [variance table, Return-arg]: for each label whose
      // terminator is a Return statement, emit
      //   Subtype(local_types[ret_loc], function_return_type)
      // in Mode::Emit. This subsumes the brittle Phase 3b
      // return-evidence binding: when the returned value carries a
      // concrete type (or contains α_k references), the Subtype
      // decomposition records the appropriate add_lower / add_upper
      // / unify constraints.
      Node func_return = func_def / Type;
      if (func_return)
      {
        auto formal_return = apply_subst(top, func_return, subst);
        for (auto& l : *(func_def / Labels))
        {
          auto term = l / Return;
          if (term != Return)
            continue;
          auto ret_loc = (term / LocalId)->location();
          auto rit = source_types.find(ret_loc);
          if (rit == source_types.end())
            continue;
          auto ret_type = apply_subst(top, rit->second, subst);
          emit_subtype(ret_type, formal_return);
        }
      }
    }

    // Phase 5 cross-reification gather: when a New/Stack of a lifted
    // lambda appears in a caller's body, walk the lifted class's
    // apply Function body and emit Store-payload constraints into
    // the (shared) typevar_store. The TypeVar identities flow via:
    //  - caller seeds r.subst[U_k] = Type(TypeVar α_k)
    //  - reify_emitted_type at the New site fills the lambda's
    //    TypeArgs with α_k via the substitution
    //  - inside apply, references to the lambda's own (cloned)
    //    TypeParam are redirected to the same α_k by
    //    rewrite_typeparam_refs (capture invariant: clone preserves
    //    Location)
    //  - Store statements in apply's body whose ref target is a
    //    captured field of type ref[Union(α_k, ...)] contribute
    //    add_lower(α_k, value_type) to the store.
    void gather_lambda_apply_constraints(
      const Node& new_type_wrapper, const NodeMap<Node>& caller_subst)
    {
      if (!new_type_wrapper)
        return;

      Node inner =
        (new_type_wrapper == Type) ? new_type_wrapper->front() : new_type_wrapper;
      if (!inner || inner != TypeName || inner->empty())
        return;

      // Find the class def for this New site's TypeName.
      auto cls_def = find_def(top, inner);
      if (!cls_def || cls_def != ClassDef)
        return;

      // Only lifted lambda classes — they have idents starting with
      // "lambda$".
      Node cls_ident = cls_def / Ident;
      if (cls_ident->location().view().rfind("lambda$", 0) != 0)
        return;

      Node cls_body = cls_def / ClassBody;

      // Find the apply Function inside this class.
      Node apply_func;
      for (auto& member : *cls_body)
      {
        if (member != Function)
          continue;
        Node fn_ident = member / Ident;
        if (fn_ident->location().view() == "apply")
        {
          apply_func = member;
          break;
        }
      }
      if (!apply_func)
        return;

      // Local apply_subst that propagates TypeVar-valued substitutions
      // (unlike vc::apply_subst which skips them). This is required so
      // unbound formals' fresh α_k identities flow from the caller into
      // the lambda's apply body, where Stores into captured-ref fields
      // emit add_lower(α_k, val_type) constraints. Outside this gather,
      // the TypeVar-skip semantics in apply_subst are still correct
      // (Phase 3.5 deferred placeholder).
      std::function<Node(const Node&, const NodeMap<Node>&)> subst_keep_tv =
        [&](const Node& type_node, const NodeMap<Node>& sub) -> Node {
        if (type_node != Type || sub.empty())
          return clone(type_node);
        auto in = type_node->front();
        if (in == TypeName)
        {
          auto def = find_def(top, in);
          if (def && def == TypeParam)
          {
            auto it = sub.find(def);
            if (it != sub.end())
              return clone(it->second);
          }
          Node new_tn = TypeName;
          for (auto& elem : *in)
          {
            Node new_ta = TypeArgs;
            for (auto& ta_child : *(elem / TypeArgs))
              new_ta << subst_keep_tv(ta_child, sub);
            new_tn << (NameElement << clone(elem / Ident) << new_ta);
          }
          return Type << new_tn;
        }
        if (in->in({Union, Isect, TupleType}))
        {
          Node new_inner = in->type();
          for (auto& child : *in)
            new_inner << subst_keep_tv(Type << clone(child), sub)->front();
          return Type << new_inner;
        }
        return clone(type_node);
      };

      // Build a substitution mapping the lambda class's TypeParams
      // to the New site's TypeArgs (after caller_subst applied,
      // preserving TypeVar identities).
      NodeMap<Node> apply_subst_map;
      Node cls_tps = cls_def / TypeParams;
      Node last_elem = inner->back();
      if (!last_elem)
        return;
      Node cls_tas = last_elem / TypeArgs;
      if (cls_tps->size() == cls_tas->size())
      {
        for (size_t i = 0; i < cls_tps->size(); i++)
        {
          auto ta_node = cls_tas->at(i);
          if (!ta_node)
            continue;
          auto ta = subst_keep_tv(ta_node, caller_subst);
          if (ta)
            apply_subst_map[cls_tps->at(i)] = ta;
        }
      }

      // Build a quick map: field name → field type.
      std::map<std::string_view, Node> field_types;
      for (auto& member : *cls_body)
      {
        if (member != FieldDef)
          continue;
        Node fid = member / Ident;
        Node ft = member / Type;
        field_types[fid->location().view()] = ft;
      }

      // Apply's params give us types for apply's locals.
      std::map<Location, Node> apply_local_types;
      Node apply_params = apply_func / Params;
      for (auto& pd : *apply_params)
      {
        Node pd_ident = pd / Ident;
        Node pd_type = pd / Type;
        auto subbed = subst_keep_tv(pd_type, apply_subst_map);
        if (subbed && subbed == Type && !subbed->empty())
          apply_local_types[pd_ident->location()] = subbed->front();
      }

      // Walk apply's body emitting Store constraints.
      Node labels = apply_func / Labels;
      for (auto& lbl : *labels)
      {
        Node body = lbl / Body;
        for (auto& stmt : *body)
        {
          if (!stmt)
            continue;
          if (stmt->in({Const, Convert}))
          {
            Node sid = stmt / LocalId;
            Node stype = stmt / Type;
            if (stype == Type && !stype->empty())
              apply_local_types[sid->location()] = clone(stype->front());
          }
          else if (stmt == TypeAssertion)
          {
            Node sid = stmt / LocalId;
            Node stype = stmt / Type;
            auto subbed = subst_keep_tv(stype, apply_subst_map);
            if (subbed && subbed == Type && !subbed->empty())
              apply_local_types[sid->location()] = clone(subbed->front());
          }
          else if (stmt->in({Copy, Move}))
          {
            Node src_id = stmt / Rhs;
            Node dst_id = stmt / LocalId;
            auto it = apply_local_types.find(src_id->location());
            if (it != apply_local_types.end())
              apply_local_types[dst_id->location()] = it->second;
          }
          else if (stmt == FieldRef)
          {
            Node fid = stmt / FieldId;
            Node did = stmt / LocalId;
            auto fname = fid->location().view();
            auto fit = field_types.find(fname);
            if (fit != field_types.end())
            {
              auto ftype = subst_keep_tv(fit->second, apply_subst_map);
              if (ftype && ftype == Type && !ftype->empty())
                apply_local_types[did->location()] =
                  Ref << clone(ftype->front());
            }
          }
          else if (stmt == Load)
          {
            Node src_id = stmt / Rhs;
            Node dst_id = stmt / LocalId;
            auto it = apply_local_types.find(src_id->location());
            if (
              it != apply_local_types.end() && it->second && it->second == Ref &&
              !it->second->empty())
              apply_local_types[dst_id->location()] =
                clone(it->second->front());
          }
          else if (stmt == Store)
          {
            Node ref_id = stmt / Rhs;
            auto ref_loc = ref_id->location();
            auto ref_it = apply_local_types.find(ref_loc);
            if (ref_it == apply_local_types.end() || !ref_it->second)
              continue;
            // Extract the ref payload. Two shapes are possible:
            //   1. Ref << <payload>  — local introduced via FieldRef
            //   2. TypeName(_builtin::ref::<n>) with TypeArgs[0] = payload
            //      — local introduced via TypeAssertion on $ref_<name>.
            Node payload;
            auto t = ref_it->second;
            if (t == Ref && !t->empty())
            {
              payload = t->front();
            }
            else if (t == TypeName && !t->empty())
            {
              Node last = t->back();
              if (last && last == NameElement)
              {
                Node tas = last / TypeArgs;
                if (tas && !tas->empty())
                {
                  Node ta = tas->front();
                  if (ta == Type && !ta->empty())
                    payload = ta->front();
                }
              }
            }
            if (!payload)
              continue;
            Node arg = stmt / Arg;
            Node val_id_node = arg / Rhs;
            auto val_loc = val_id_node->location();
            auto val_it = apply_local_types.find(val_loc);
            if (val_it == apply_local_types.end() || !val_it->second)
              continue;
            auto val_type = val_it->second;

            if (payload == TypeVar)
            {
              auto alpha_id = typevar_store.intern(payload);
              typevar_store.add_lower(alpha_id, val_type);
            }
            else if (payload == Union)
            {
              Node tv_arm;
              bool matched_concrete = false;
              for (auto& arm : *payload)
              {
                if (!arm)
                  continue;
                if (arm == TypeVar)
                {
                  if (!tv_arm)
                    tv_arm = arm;
                  else
                    tv_arm = {};
                }
                else if (val_type && val_type->equals(arm))
                {
                  matched_concrete = true;
                  break;
                }
              }
              if (!matched_concrete && tv_arm)
              {
                auto alpha_id = typevar_store.intern(tv_arm);
                typevar_store.add_lower(alpha_id, val_type);
              }
            }
          }
        }
      }
    }

    // Drive the NodeWorker until all reifications are Resolved (or
    // Blocked due to mutual recursion — Phase 3b.4 handles that).
    // Each find_or_push call may add new work items via worker->add()
    // during processing; the worker's worklist handles re-entry.
    void drain_worklist()
    {
      worker->run();
    }

    // Per-Reification dispatcher. Called once per work item by
    // ReifyWork::process. Mirrors the legacy drain_worklist body.
    // For Functions, this is the FULL pipeline (stage 1 + stage 2);
    // process() handles the stage gating with NodeWorker block_on.
    void process_reification(Reification& r, const Node& reif_id = {})
    {
      if (reif_id)
        reify_id_stack.push_back(reif_id);
      auto pop = [&]() {
        if (reif_id)
          reify_id_stack.pop_back();
      };

      if (!r.def)
      {
        pop();
        return;
      }

      if (r.def == ClassDef)
      {
        reify_class(r);
        pop();
      }
      else if (r.def == TypeAlias)
      {
        reify_typealias(r);
        pop();
      }
      else if (r.def == Function)
      {
        reify_function(r);

        // If the function had a TypeVar return in the def, defer it
        // for a second pass when all callees are reified. The first
        // pass may have produced a partial return type (e.g., nomatch
        // from match arms when the main arm's CallDyn wasn't tracked).
        if (r.reification && (r.def / Type)->front() == TypeVar)
          deferred_typevar.push_back(&r);
        pop();
      }
      else
      {
        pop();
        assert(false);
      }
    }

    void resolve_shapes()
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
              // Skip dedup'd entries whose IR was cleared by the
              // post-fixpoint dedup pass. Their ID is redirected
              // to the canonical entry; including them here would
              // produce stale references.
              if (!cr.reification)
                continue;

              auto cache_key =
                std::make_pair(cr.resolved_name.get(), r.resolved_name.get());
              auto [cache_it, inserted] =
                shape_subtype_cache.try_emplace(cache_key, false);

              if (inserted && Subtype(ctx, cr.resolved_name, r.resolved_name))
              {
                cache_it->second = true;
              }

              if (cache_it->second)
                union_node << clone(cr.id);
            }
          }

          if (union_node->empty())
          {
            // No concrete classes implement this shape. Emit an empty
            // Type entry so TypeId references are valid. The typetest
            // for this shape will always fail at runtime (no value of
            // this type can exist), making the code path unreachable.
            r.reification = Type << clone(r.id) << Union;
          }
          else if (union_node->size() == 1)
            r.reification = Type << clone(r.id) << union_node->front();
          else
            r.reification = Type << clone(r.id) << union_node;
        }
      }
    }

    // After resolve_shapes, prune TypeId members from unions in class fields
    // where the shape resolved to an empty union (no implementors). These
    // TypeIds were kept as "pending" during reify_type but now have definitive
    // empty-union reifications.
    void prune_empty_shape_unions()
    {
      auto is_empty_shape = [&](const Node& type_id) -> bool {
        for (auto& key : map_order)
        {
          for (auto& cr : map[key])
          {
            if (cr.id && same_reification_id(cr.id, type_id))
            {
              if (cr.reification && (cr.reification == Type))
              {
                auto& body = cr.reification->back();
                return (body == Union) && body->empty();
              }

              return false;
            }
          }
        }

        return true;
      };

      // Prune a union node in place, returning the simplified result.
      auto prune_union = [&](const Node& u) -> Node {
        Node result = Union;

        for (auto& child : *u)
        {
          if ((child == TypeId) && is_empty_shape(child))
            continue;

          result << clone(child);
        }

        if (result->empty())
          return Dyn;

        if (result->size() == 1)
          return result->front();

        return result;
      };

      // Canonicalize a type node: bare TypeIds that resolve to empty-Union
      // shapes are canonicalized to Dyn (no value can inhabit them, so any
      // layout is correct, and Dyn matches the catchall layout used
      // elsewhere). Unions are pruned via prune_union. Returns the
      // canonical replacement (which may be the original node).
      auto canonicalize = [&](const Node& t) -> Node {
        if (!t)
          return t;

        if ((t == TypeId) && is_empty_shape(t))
          return Dyn;

        if (t == Union)
          return prune_union(t);

        return t;
      };

      auto canonicalize_in_place = [&](Node parent, const Node& child) {
        auto pruned = canonicalize(child);

        if (pruned != child)
          parent->replace(child, pruned);
      };

      for (auto& key : map_order)
      {
        for (auto& r : map[key])
        {
          if (!r.reification)
            continue;

          if (r.reification == Class)
          {
            auto fields = r.reification / Fields;

            for (auto& field : *fields)
              canonicalize_in_place(field, field->back());
          }
          else if (r.reification == Type)
          {
            canonicalize_in_place(r.reification, r.reification->back());
          }
          else if (r.reification->in({Func, FuncOnce}))
          {
            // Function param types and return type. The return type is
            // the third child (FunctionId * Params * Type * Vars * Labels).
            for (auto& p : *(r.reification / Params))
              canonicalize_in_place(p, p->back());

            // Return type position: index 2.
            auto ret = r.reification->at(2);
            canonicalize_in_place(r.reification, ret);
          }
        }
      }
    }

    struct ReceiverSet
    {
      bool all;
      Nodes types;
    };

    Node resolve_receiver_typeid(const Node& type_id)
    {
      if (!type_id || (type_id != TypeId))
        return {};

      auto type_id_loc = type_id->location().view();

      for (auto& key : map_order)
      {
        for (auto& r : map[key])
        {
          if (!r.id || (r.id->location().view() != type_id_loc))
            continue;

          if (!r.reification && (r.def == TypeAlias))
            reify_typealias(r);

          if (r.reification && (r.reification == Type))
            return clone(r.reification->back());

          return {};
        }
      }

      return {};
    }

    Node resolve_reified_typeids(const Node& type)
    {
      if (!type)
        return {};

      if (type == TypeId)
      {
        auto resolved = resolve_receiver_typeid(type);
        return resolved ? resolve_reified_typeids(resolved) : clone(type);
      }

      if (type == Type)
        return Type << resolve_reified_typeids(type->front());

      if (type == Union)
      {
        Node result = Union;

        for (auto& child : *type)
          result << resolve_reified_typeids(child);

        return result;
      }

      if (type->in({Ref, Array, Cown}))
        return type->type() << resolve_reified_typeids(type->front());

      return clone(type);
    }

    // Extract individual type ids from a reified type. Dyn (or an unresolved
    // shape TypeId) means "all classes/primitives". A resolved shape TypeId is
    // expanded to its concrete implementations. Wrapper receivers are expanded
    // across the resolved inner receiver set.
    ReceiverSet extract_receivers(const Node& reified_type)
    {
      auto same_receiver = [](const Node& left, const Node& right) {
        if (!left || !right || (left->type() != right->type()))
          return false;

        if (left->in({ClassId, FunctionId, TypeId}))
          return left->location().view() == right->location().view();

        Node left_id = left;
        Node right_id = right;
        return left_id->equals(right_id);
      };

      std::function<ReceiverSet(const Node&)> collect = [&](const Node& type) {
        if (!type || (type == Dyn))
          return ReceiverSet{true, {}};

        if (type == Type)
          return collect(type->front());

        if (type == TypeId)
        {
          auto resolved = resolve_receiver_typeid(type);
          return resolved ? collect(resolved) : ReceiverSet{true, {}};
        }

        if (type == Union)
        {
          ReceiverSet explicit_result{false, {}};
          ReceiverSet expanded_result{false, {}};
          bool saw_expanded = false;

          auto add_unique = [&](ReceiverSet& result, Node recv) {
            bool dup = false;

            for (auto& existing : result.types)
            {
              if (same_receiver(existing, recv))
              {
                dup = true;
                break;
              }
            }

            if (!dup)
              result.types.push_back(clone(recv));
          };

          for (auto& child : *type)
          {
            auto child_set = collect(child);

            if (child_set.all)
              return ReceiverSet{true, {}};

            auto& result =
              contains_typeid(child) ? expanded_result : explicit_result;
            saw_expanded |= contains_typeid(child);

            for (auto& recv : child_set.types)
              add_unique(result, recv);
          }

          if (saw_expanded && !explicit_result.types.empty())
          {
            bool explicit_subset = true;

            for (auto& recv : explicit_result.types)
            {
              bool found = false;

              for (auto& expanded : expanded_result.types)
              {
                if (same_receiver(expanded, recv))
                {
                  found = true;
                  break;
                }
              }

              if (!found)
              {
                explicit_subset = false;
                break;
              }
            }

            if (explicit_subset)
              return explicit_result;
          }

          for (auto& recv : expanded_result.types)
            add_unique(explicit_result, recv);

          return explicit_result;
        }

        if (type->in({Ref, Array, Cown}))
        {
          auto inner_set = collect(type->front());

          if (inner_set.all)
            return ReceiverSet{true, {}};

          ReceiverSet result{false, {}};

          for (auto& inner : inner_set.types)
          {
            Node wrapper = type->type() << clone(inner);
            result.types.push_back(wrapper);
          }

          return result;
        }

        return ReceiverSet{false, {clone(type)}};
      };

      return collect(reified_type);
    }

    bool same_reification_id(const Node& left, const Node& right)
    {
      if (!left || !right || (left->type() != right->type()))
        return false;

      if (left->in({ClassId, FunctionId, TypeId}))
        return left->location().view() == right->location().view();

      Node left_id = left;
      Node right_id = right;
      return left_id->equals(right_id);
    }

    // Check if a MethodInvocation targets a specific class reification.
    bool mi_targets(const MethodInvocation& mi, Node class_id)
    {
      if (mi.all_receivers)
        return true; // all classes

      for (auto r : mi.receivers)
      {
        if (same_reification_id(class_id, r))
          return true;
      }

      return false;
    }

    // bake_typename is below; resolve_typearg calls it. They are
    // mutually recursive: bake_typename copies in values from subst
    // via clone(resolve_typearg(...)) so that each filled slot is
    // self-contained.

    // Walk a fully-qualified TypeName per-element and produce a
    // canonical, self-contained form: for every NameElement whose def
    // has TypeParams but whose TypeArgs is empty (or contains only
    // a deferred Type(TypeVar) placeholder), fill the TypeArgs from
    // `subst`. The filled values are themselves canonicalized through
    // resolve_typearg so that no implicit-TypeArg slot remains in the
    // result. Existing explicit TypeArgs are also recursively
    // canonicalized.
    //
    // The "self-contained" property means: a baked TypeName can be
    // stored across a reification boundary (into another reification's
    // subst map) without losing its meaning; downstream readers do
    // not need ambient context to interpret it.
    //
    // Per-leaf tri-state at fill points:
    //   - bound: subst has a concrete value for the TypeParam — bake.
    //   - deferred: subst has Type(TypeVar) — emit Type(TypeVar) at
    //     the slot; outer structure preserved.
    //   - error: subst lacks the TypeParam — hard compile error per
    //     the Dyn-rule (Dyn is ONLY the IR encoding of `any`, never
    //     a fallback).
    //
    // Fast path: if no slot was filled and no explicit child changed,
    // returns the original TypeName unchanged (no clone).
    Node bake_typename(const Node& tn, const NodeMap<Node>& subst)
    {
      assert(tn->in({TypeName, FuncName}));

      Node def = top;
      Node baked = tn->type();
      bool changed = false;

      for (auto it = tn->begin(); it != tn->end(); ++it)
      {
        auto& elem = *it;
        assert(elem == NameElement);
        auto ident = elem / Ident;
        auto ta = elem / TypeArgs;
        bool is_last = (it + 1 == tn->end());

        auto defs = def->look(ident->location());

        if (defs.empty())
        {
          // Cannot navigate. Bail out and return original (preserves
          // existing behavior for malformed names; errors will be
          // reported elsewhere).
          return tn;
        }

        // Disambiguate when multiple defs share a name (e.g. function
        // overloads): pick the one whose body contains the next
        // element. Mirrors get_reification's logic.
        Node next = {};

        if (defs.size() > 1 && !is_last)
        {
          auto next_ident = (*(it + 1)) / Ident;
          for (auto& d : defs)
          {
            if (!d->look(next_ident->location()).empty())
            {
              next = d;
              break;
            }
          }
        }

        if (!next)
          next = defs.front();

        // For non-last NameElements we expect a navigable scope. Bail
        // (return original) on non-navigable shapes; errors are
        // reported by get_reification when it does its own walk.
        if (!is_last && !next->in({ClassDef, Function, TypeAlias}))
          return tn;

        // For TypeParam at the last position, we don't bake — the
        // caller (resolve_typearg) handles that case directly.
        if (next == TypeParam)
        {
          if (changed)
            baked << clone(elem);
          def = next;
          continue;
        }

        auto tps = next / TypeParams;
        Node new_ta;
        bool elem_changed = false;

        // Treat a TypeArgs slot as "logically empty" if its only
        // child is a deferred Type(TypeVar) placeholder. This
        // matches the explicit-TypeArg deferral at lines ~5446 of
        // get_reification, and lets a subsequent bake fill the slot
        // when subst gains a binding.
        bool ta_logically_empty = ta->empty();

        if (
          !ta->empty() && (ta->size() == tps->size()) &&
          std::all_of(ta->begin(), ta->end(), [](const Node& a) {
            return (a == Type) && (a->front() == TypeVar);
          }))
        {
          ta_logically_empty = true;
        }

        if (ta_logically_empty && !tps->empty())
        {
          // Implicit slot at an intermediate or final scope with
          // TypeParams. Fill from subst per TypeParam.
          new_ta = TypeArgs;
          elem_changed = true;

          for (auto& tp : *tps)
          {
            auto find = subst.find(tp);

            if (find == subst.end())
            {
              // Unbound formal at this position. Emit a deferred
              // Type(TypeVar) placeholder rather than a hard error
              // here — bake_typename can be called many times along
              // overlapping paths, so an error per call would
              // multiply diagnostics. The proper error is emitted
              // by resolve_typearg / get_reification when the
              // unbound TypeParam is actually used.
              new_ta << (Type << make_typevar());
              continue;
            }

            // Canonicalize the value before injecting (closes the
            // substitution: any TypeName inside `find->second` with
            // implicit slots gets baked via resolve_typearg, which
            // recurses back into bake_typename for nested TypeNames).
            new_ta << clone(resolve_typearg(find->second, subst));
          }
        }
        else if (!ta->empty())
        {
          // Explicit TypeArgs: recursively canonicalize each child.
          new_ta = TypeArgs;

          for (auto& a : *ta)
          {
            auto resolved = resolve_typearg(a, subst);

            if (resolved != a)
              elem_changed = true;

            new_ta << clone(resolved);
          }
        }

        if (elem_changed)
        {
          if (!changed)
          {
            // Lazily copy already-walked elements we passed through
            // unchanged.
            for (auto it2 = tn->begin(); it2 != it; ++it2)
              baked << clone(*it2);
            changed = true;
          }
          baked << (NameElement << clone(ident) << new_ta);
        }
        else if (changed)
        {
          baked << clone(elem);
        }

        def = next;
      }

      return changed ? baked : tn;
    }

    // Resolve a TypeArg through the current substitution map. If the TypeArg
    // is a Type wrapping a TypeName that resolves to a TypeParam in the subst,
    // return the substituted value. Otherwise, return the original TypeArg.
    Node resolve_typearg(const Node& arg, const NodeMap<Node>& subst)
    {
      bool wrapped = (arg == Type);
      auto inner = arg;

      // Unwrap Type node.
      if (wrapped)
        inner = inner->front();

      if (inner == Union)
      {
        Node r = Union;
        bool has_non_dyn = false;

        for (auto& child : *inner)
        {
          auto resolved = resolve_typearg(child, subst);
          auto resolved_inner =
            (resolved == Type) ? resolved->front() : resolved;
          if (resolved_inner != Dyn)
            has_non_dyn = true;
          r << clone(resolved_inner);
        }

        if (has_non_dyn)
        {
          Node filtered = Union;
          for (auto& child : *r)
          {
            if (child != Dyn)
              filtered << clone(child);
          }
          r = filtered;
        }

        if (r->size() == 1)
          return wrapped ? (Type << clone(r->front())) : clone(r->front());

        return wrapped ? (Type << r) : r;
      }

      if (inner != TypeName)
        return arg;

      // Navigate the FQ name to see if the last element is a TypeParam.
      auto def = find_def(top, inner);

      if (!def)
        return arg;

      if (def == TypeParam)
      {
        // It's a TypeParam. Look it up in the subst map.
        auto find = subst.find(def);

        if (find != subst.end())
        {
          if (wrapped)
            return clone(find->second);

          return (find->second == Type) ? clone(find->second->front()) :
                                          clone(find->second);
        }

        // TypeParam not in subst — this is a hard compile error. Dyn is
        // ONLY the IR encoding of `any`; it must never be used as a
        // fallback for an unbound formal. Emit the error and return Dyn
        // so the AST stays well-formed for downstream cleanup; the compile
        // will fail at end of pass because errors is non-empty.
        if (reported_unbound_formal.insert(def.get()).second)
        {
          errors.push_back(err(
            inner,
            std::format(
              "Type parameter `{}` cannot be inferred. Provide an explicit "
              "type argument.",
              (def / Ident)->location().view())));
        }
        return wrapped ? (Type << Dyn) : Dyn;
      }

      // Not a bare TypeParam. Recursively bake the TypeName: fills
      // implicit TypeArgs at intermediate scopes from `subst`, and
      // recursively canonicalises any explicit TypeArgs. The result
      // is self-contained — safe to store across a reification
      // boundary without losing context.
      auto baked = bake_typename(inner, subst);

      if (baked == inner)
        return arg;

      // Re-wrap in Type if the original was wrapped.
      if (wrapped)
        return Type << baked;

      return baked;
    }

    // Compare two subst values for equivalence, including α-equivalence
    // for TypeVar leaves (two TypeVar leaves with the same union-find
    // root are equivalent). Used by find_or_push dedup so that callees
    // scheduled with TypeVar substs from the same enclosing reification
    // dedup correctly.
    bool subst_value_equal(const Node& lhs, const Node& rhs)
    {
      // Both bare TypeVar leaves: α-equivalence.
      if (lhs && rhs && lhs == TypeVar && rhs == TypeVar)
      {
        auto la = typevar_store.intern(lhs);
        auto ra = typevar_store.intern(rhs);
        return typevar_store.root(la) == typevar_store.root(ra);
      }
      // Type-wrapped: unwrap and recurse if either side wraps a TypeVar.
      if (lhs && rhs && lhs == Type && rhs == Type &&
          !lhs->empty() && !rhs->empty() &&
          (lhs->front() == TypeVar || rhs->front() == TypeVar))
        return subst_value_equal(lhs->front(), rhs->front());
      // Default: invariant subtype.
      return Subtype.invariant(top, lhs, rhs);
    }

    // Check whether two substitution maps are equivalent under invariance.
    bool subst_equal(const NodeMap<Node>& a, const NodeMap<Node>& b)
    {
      if (a.size() != b.size())
        return false;

      return std::equal(
        a.begin(), a.end(), b.begin(), b.end(), [&](auto& lhs, auto& rhs) {
          return (lhs.first == rhs.first) &&
            subst_value_equal(lhs.second, rhs.second);
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
    Node
    find_or_push(const Node& def, NodeMap<Node> subst, Node resolved_name = {})
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
            {
              // If the existing entry has an empty subst but the new call
              // has a non-empty one, update the existing entry's subst.
              // This happens when ensure_ref_reified creates a wrapper with
              // empty subst, and get_reification later provides proper subst.
              if (existing.subst.empty() && !subst.empty())
              {
                existing.subst = subst;

                // Re-register methods now that subst is available.
                if (existing.reification)
                {
                  for (auto& mi : method_invocations)
                  {
                    if (mi_targets(mi, existing.id))
                      register_method(mi, existing);
                  }
                }
              }

              if (!existing.resolved_name && resolved_name)
                existing.resolved_name = resolved_name;

              auto out = clone(existing.id);
              record_dep(out);
              return out;
            }
          }

          r_vec.push_back(
            {def,
             std::move(subst),
             std::move(id),
             {},
             std::move(resolved_name)});
          register_with_worker(&r_vec.back());
          auto out = clone(r_vec.back().id);
          record_dep(out);
          return out;
        }
      }

      // All other defs: dedup using substitution map equality.
      // Compare entries for TypeParams owned by this def AND by ALL
      // enclosing scopes (ClassDef and Function). Nested classes/
      // functions can reference any ancestor's TypeParams in their
      // own signatures/bodies, so different bindings for any
      // enclosing scope must produce different reifications. Walk
      // the full ancestor chain through both ClassDef and Function
      // boundaries — a nested def captured inside a generic function
      // (e.g. a lambda lifted from `f[U]`) dedupes by `f`'s U too.
      auto own_tps = def / TypeParams;
      Nodes parent_tps_chain;
      for (auto p = def->parent({ClassDef, Function}); p;
           p = p->parent({ClassDef, Function}))
      {
        for (auto& tp : *(p / TypeParams))
          parent_tps_chain.push_back(tp);
      }

      for (auto& existing : r_vec)
      {
        bool match = true;

        auto check_tp = [&](const Node& tp) {
          auto a_it = existing.subst.find(tp);
          auto b_it = subst.find(tp);

          if (a_it == existing.subst.end() && b_it == subst.end())
            return;

          if (a_it == existing.subst.end() || b_it == subst.end())
          {
            match = false;
            return;
          }

          if (!subst_value_equal(a_it->second, b_it->second))
          {
            match = false;
          }
        };

        for (auto& tp : *own_tps)
        {
          check_tp(tp);
          if (!match)
            break;
        }

        if (match)
        {
          for (auto& tp : parent_tps_chain)
          {
            check_tp(tp);
            if (!match)
              break;
          }
        }

        if (match)
        {
          if (!existing.resolved_name && resolved_name)
            existing.resolved_name = resolved_name;
          auto out = clone(existing.id);
          record_dep(out);
          return out;
        }
      }

      auto id = make_id(def, r_vec.size(), subst);

      r_vec.push_back(
        {def, std::move(subst), std::move(id), {}, std::move(resolved_name)});
      register_with_worker(&r_vec.back());
      auto out = clone(r_vec.back().id);
      record_dep(out);
      return out;
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

          auto find_create_param_type = [&](const Node& field_def) -> Node {
            auto field_name = (field_def / Ident)->location().view();

            for (auto& child : *(r.def / ClassBody))
            {
              if (child != Function)
                continue;

              if ((child / Ident)->location().view() != "create")
                continue;

              auto* create_reif = find_function_reification(child, r.subst);

              if (!create_reif || !create_reif->reification)
                continue;

              auto params = create_reif->reification / Params;
              auto def_params = child / Params;

              for (size_t i = 0; i < def_params->size(); i++)
              {
                if (
                  (def_params->at(i) / Ident)->location().view() != field_name)
                  continue;

                return clone(params->at(i) / Type);
              }
            }

            return {};
          };

          auto has_create_param = [&](const Node& field_def) {
            auto field_name = (field_def / Ident)->location().view();

            for (auto& child : *(r.def / ClassBody))
            {
              if (child != Function)
                continue;

              if ((child / Ident)->location().view() != "create")
                continue;

              for (auto& param : *(child / Params))
              {
                if ((param / Ident)->location().view() == field_name)
                  return true;
              }
            }

            return false;
          };

          for (auto& f : *(r.def / ClassBody))
          {
            if (f != FieldDef)
              continue;

            auto field_type = find_create_param_type(f);

            if (!field_type)
            {
              field_type = has_create_param(f) ?
                reify_type(f / Type, r.subst) :
                reify_emitted_type(f / Type, r.subst, f / Ident, "field type");
            }

            fields << (Field << (FieldId ^ (f / Ident)) << field_type);
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

      // Register @final method if the class has a `final` function.
      for (auto& f : *(r.def / ClassBody))
      {
        if (f != Function)
          continue;

        if ((f / Ident)->location().view() != "final")
          continue;

        if ((f / Lhs)->type() != Rhs)
          continue;

        if (!((f / TypeParams)->empty()))
          continue;

        if ((f / Params)->size() != 1)
          continue;

        // Reify the final function and register as @final.
        auto funcid = find_or_push(f, r.subst);
        auto methods = r.reification / Methods;
        methods << (Method << (MethodId ^ "@final") << funcid);
        break;
      }
    }

    void reify_typealias(Reification& r)
    {
      // Store the reified type alias as a Type entry (TypeAlias is
      // resolved during reification; the IR uses Type for lookups).
      r.reification =
        Type << r.id
             << reify_emitted_type(
                  r.def / Type, r.subst, r.def / Ident, "type alias");
    }

    // Find a function reification by FunctionId and return its reified
    // return type. Returns empty Node if not found or not yet reified.
    Node find_func_return_type(const Node& funcid)
    {
      auto funcid_loc = funcid->location().view();

      for (auto& key : map_order)
      {
        if (key != Function)
          continue;

        for (auto& reif : map[key])
        {
          if (reif.id && (reif.id->location().view() == funcid_loc))
          {
            if (reif.reification)
              return clone(reif.reification / Type);

            // Not yet reified — eagerly compute the return type from the
            // function's ClassDef so that receiver-type tracking for
            // downstream Lookup/CallDyn sites doesn't fall back to the
            // conservative "all classes" receiver set.
            auto def_type = reif.def / Type;
            if (def_type->front() == TypeVar)
              return {};

            // Phase 3b.4: if the def references unbound formals (subst
            // missing entries) — body-driven binding will fill these
            // when reify_function runs later. Don't eagerly reify here;
            // it would emit a spurious "type parameter cannot be
            // inferred" error before binding happens. Returning {}
            // lets the caller fall back to TypeVar / unresolved-receiver
            // tracking; the second-pass refinement (after binding
            // populates reif.reification) replaces it with the actual
            // return type.
            if (has_unresolved_type(def_type, reif.subst))
              return {};

            return reify_emitted_type(
              def_type, reif.subst, reif.def / Ident, "return type");
          }
        }
      }

      return {};
    }

    Reification* find_function_reification(const Node& funcid)
    {
      auto funcid_loc = funcid->location().view();

      for (auto& key : map_order)
      {
        if (key != Function)
          continue;

        for (auto& reif : map[key])
        {
          if (reif.id && (reif.id->location().view() == funcid_loc))
            return &reif;
        }
      }

      return nullptr;
    }

    Reification*
    find_function_reification(const Node& def, const NodeMap<Node>& subst)
    {
      for (auto& key : map_order)
      {
        if (key != Function)
          continue;

        for (auto& reif : map[key])
        {
          if ((reif.def == def) && subst_equal(reif.subst, subst))
            return &reif;
        }
      }

      return nullptr;
    }

    Reification* find_class_reification(const Node& classid)
    {
      auto classid_loc = classid->location().view();

      for (auto& key : map_order)
      {
        if (key != ClassDef)
          continue;

        for (auto& reif : map[key])
        {
          if (reif.id && (reif.id->location().view() == classid_loc))
            return &reif;
        }
      }

      return nullptr;
    }

    std::vector<Reification*> find_method_targets(
      Node recv_type,
      const std::string& method_id,
      const Node& args,
      bool behavior_args)
    {
      auto recv_set = extract_receivers(recv_type);
      std::vector<Reification*> targets;

      for (auto& key : map_order)
      {
        if (key != ClassDef)
          continue;

        for (auto& r : map[key])
        {
          if (!r.reification || !r.id)
            continue;

          if ((r.def / Shape) == Shape)
            continue;

          bool matches = recv_set.all;

          for (auto& recv : recv_set.types)
          {
            if (same_reification_id(r.id, recv))
            {
              matches = true;
              break;
            }
          }

          if (!matches)
            continue;

          auto methods = r.reification / Methods;

          for (auto& m : *methods)
          {
            if ((m / MethodId)->location().view() != method_id)
              continue;

            auto* target = find_function_reification(m / FunctionId);

            if (
              target &&
              method_target_accepts_args(*target, args, behavior_args) &&
              std::none_of(targets.begin(), targets.end(), [&](auto* existing) {
                return existing == target;
              }))
            {
              targets.push_back(target);
            }

            break;
          }
        }
      }

      return targets;
    }

    // Remove `to_remove` from `source`. Returns the residual type, or null
    // if no narrowing applies (e.g., source doesn't contain to_remove).
    // Operates on IR-level types.
    Node subtract_type(const Node& source, const Node& to_remove)
    {
      if (!source || !to_remove)
        return {};

      if (source == Dyn)
        return {};

      if (source == Union)
      {
        Node remaining = Union;
        bool removed = false;

        for (auto& child : *source)
        {
          if (vbcc::IRSubtype.invariant(top, child, to_remove))
          {
            removed = true;
          }
          else
          {
            remaining << clone(child);
          }
        }

        if (!removed)
          return {};

        if (remaining->empty())
          return Union;

        if (remaining->size() == 1)
          return clone(remaining->front());

        return remaining;
      }

      if (vbcc::IRSubtype.invariant(top, source, to_remove))
        return Union;

      return {};
    }

    // Walk a label body in reverse to find the Typetest that produced
    // `cond_local`'s value, following any Not chain. Returns
    // {src_loc, type, negated} on success.
    struct TypetestTrace
    {
      Location src_loc;
      Node type;
      bool negated;
    };

    std::optional<TypetestTrace>
    trace_typetest(const Location& cond_local, const Node& body)
    {
      Location target_loc = cond_local;
      bool negated = false;

      for (auto it = body->rbegin(); it != body->rend(); ++it)
      {
        auto& stmt = *it;

        if (stmt == Not)
        {
          if ((stmt / LocalId)->location() == target_loc)
          {
            target_loc = (stmt / Rhs)->location();
            negated = !negated;
          }
        }
        else if (stmt == Typetest)
        {
          if ((stmt / LocalId)->location() == target_loc)
          {
            return TypetestTrace{
              (stmt / Rhs)->location(), stmt / Type, negated};
          }
        }
      }

      return std::nullopt;
    }

    // Edge from a predecessor label to a successor, optionally carrying
    // typetest-induced narrowing on a single local.
    struct EdgeNarrowing
    {
      Location src_loc;
      Node test_type;
      // If true, the edge confirms src is NOT test_type (subtract).
      // If false, the edge confirms src IS test_type (intersect).
      bool exclude;
    };

    struct LabelEdge
    {
      Location pred_loc;
      std::optional<EdgeNarrowing> narrowing;
    };

    using LabelPredMap = std::map<Location, std::vector<LabelEdge>>;

    LabelPredMap build_label_pred_map(const Node& labels)
    {
      LabelPredMap preds;

      for (auto& lbl : *labels)
      {
        Location pred_loc = (lbl / LabelId)->location();
        auto term = lbl / Return;

        if (term == Jump)
        {
          Location succ = (term / LabelId)->location();
          preds[succ].push_back({pred_loc, std::nullopt});
        }
        else if (term == Cond)
        {
          Location bool_loc = (term / LocalId)->location();
          Location t_succ = (term / Lhs)->location();
          Location f_succ = (term / Rhs)->location();

          auto trace = trace_typetest(bool_loc, lbl / Body);

          std::optional<EdgeNarrowing> t_narrow;
          std::optional<EdgeNarrowing> f_narrow;

          if (trace)
          {
            // True branch: cond bool was true. If trace not negated, the
            // test was Typetest(_, src, type) directly, so true means
            // src IS type. If negated (via Not), true means src is NOT
            // type.
            if (!trace->negated)
            {
              t_narrow = EdgeNarrowing{trace->src_loc, trace->type, false};
              f_narrow = EdgeNarrowing{trace->src_loc, trace->type, true};
            }
            else
            {
              t_narrow = EdgeNarrowing{trace->src_loc, trace->type, true};
              f_narrow = EdgeNarrowing{trace->src_loc, trace->type, false};
            }
          }

          preds[t_succ].push_back({pred_loc, t_narrow});
          preds[f_succ].push_back({pred_loc, f_narrow});
        }
        // Return, Raise, Tailcall, TailcallDyn: no successors.
      }

      return preds;
    }

    using LocalEnv = std::map<Location, Node>;

    // Apply edge narrowing to a copy of the predecessor's exit env.
    LocalEnv apply_edge_narrowing(
      LocalEnv env, const std::optional<EdgeNarrowing>& narrowing)
    {
      if (!narrowing)
        return env;

      auto it = env.find(narrowing->src_loc);
      if (it == env.end())
        return env;

      Node narrowed;
      if (narrowing->exclude)
      {
        narrowed = subtract_type(it->second, narrowing->test_type);
      }
      else
      {
        // Confirm src is the test type. Only intersect (use test_type)
        // if it's a subtype of the current env type — otherwise the
        // source disagrees with the test, so we skip narrowing.
        // Use one-way subtype (test_type <: current); invariant would
        // require dyn <: test_type which is false for any concrete type
        // and would silently skip narrowing whenever the current type is
        // dyn (e.g., a lambda parameter that has just been typetested).
        if (vbcc::IRSubtype(top, narrowing->test_type, it->second))
          narrowed = clone(narrowing->test_type);
      }

      if (narrowed)
        it->second = narrowed;

      return env;
    }

    // Merge the entry env from a predecessor into the accumulating env.
    void merge_env_into(LocalEnv& dst, const LocalEnv& src)
    {
      for (auto& [loc, ty] : src)
      {
        auto it = dst.find(loc);
        if (it == dst.end())
          dst[loc] = clone(ty);
        else
          dst[loc] = merge_refined_type(it->second, ty);
      }
    }

    bool envs_equal(const LocalEnv& a, const LocalEnv& b)
    {
      if (a.size() != b.size())
        return false;

      for (auto& [loc, ty] : a)
      {
        auto it = b.find(loc);
        if (it == b.end())
          return false;
        Node tmp = it->second;
        if (!ty->equals(tmp))
          return false;
      }

      return true;
    }

    Node merge_refined_type(Node current, Node actual)
    {
      if (!current)
        return clone(actual);

      if (!actual)
        return clone(current);

      if (current->equals(actual))
        return clone(current);

      // TypeVar is the inference unknown ("no observation yet").
      // When merging with any real type T, prefer T (TypeVar is bottom
      // on the inference lattice).
      if (current == TypeVar)
        return clone(actual);

      if (actual == TypeVar)
        return clone(current);

      // Note: Dyn is the type-system top, not a placeholder. For
      // proper join semantics, merge(Dyn, T) = Dyn | T = Dyn (any
      // T <: Dyn). The previous v23/v24 code had an inversion here
      // that returned T when current was Dyn — that was the
      // Dyn-as-placeholder semantics now replaced by TypeVar-as-
      // placeholder. The merge below produces Dyn naturally because
      // adding T to a union containing Dyn yields {Dyn, T}; we
      // simplify by returning Dyn directly when either side is Dyn.
      if (current == Dyn || actual == Dyn)
        return Dyn;

      Node merged = Union;

      auto add_one = [&](Node single) {
        for (auto& existing : *merged)
        {
          if (existing->equals(single))
            return;
        }

        merged << clone(single);
      };

      // Flatten nested unions so the resulting union is canonical
      // (single-level). Without this, a union child is added as a single
      // element each round; subsequent merges re-add a structurally-equal
      // but non-deduplicated nested union, and the merged size grows
      // without bound across the reify fixpoint.
      std::function<void(Node)> add_type = [&](Node type) {
        if (type == Union)
        {
          for (auto& child : *type)
            add_type(child);
        }
        else
        {
          add_one(type);
        }
      };

      add_type(current);
      add_type(actual);

      if (merged->empty())
        return Dyn;

      if (merged->size() == 1)
        return clone(merged->front());

      return merged;
    }

    Node behavior_arg_type(Node type)
    {
      if (!type)
        return {};

      if (type == Union)
      {
        Node converted = Union;

        auto add_unique = [&](Node candidate) {
          if (!candidate)
            return;

          auto add_one = [&](Node single) {
            for (auto& existing : *converted)
            {
              if (existing->equals(single))
                return;
            }

            converted << clone(single);
          };

          if (candidate == Union)
          {
            for (auto& child : *candidate)
              add_one(child);
          }
          else
          {
            add_one(candidate);
          }
        };

        for (auto& child : *type)
          add_unique(behavior_arg_type(child));

        if (converted->empty())
          return {};

        if (converted->size() == 1)
          return clone(converted->front());

        return converted;
      }

      if (type != Cown)
        return clone(type);

      auto inner = clone(type->front());

      if (inner != Dyn)
        ensure_ref_reified(inner);

      return Ref << inner;
    }

    Node actual_arg_type(const Node& arg, bool behavior_arg)
    {
      auto src = arg->back();
      auto it = local_types.find(src->location());

      if (it == local_types.end())
        return {};

      if (!behavior_arg)
        return clone(it->second);

      return behavior_arg_type(it->second);
    }

    bool method_target_accepts_args(
      Reification& target, const Node& args, bool behavior_args)
    {
      if (!target.reification)
        return false;

      auto params = target.reification / Params;

      if (params->size() != args->size())
        return false;

      for (size_t i = 1; i < args->size(); i++)
      {
        auto actual = actual_arg_type(args->at(i), behavior_args);

        if (!actual)
          continue;

        auto param = params->at(i) / Type;
        auto resolved_actual = resolve_reified_typeids(actual);
        auto resolved_param = resolve_reified_typeids(param);

        if (
          contains_dyn(resolved_actual) || contains_typeid(resolved_actual) ||
          contains_dyn(resolved_param) || contains_typeid(resolved_param))
          continue;

        if (!vbcc::IRSubtype(top, resolved_actual, resolved_param))
          return false;
      }

      return true;
    }

    Node receiver_type_from_targets(const std::vector<Reification*>& targets)
    {
      Node union_node = Union;

      auto add_unique = [&](Node type) {
        for (auto& existing : *union_node)
        {
          if (existing->equals(type))
            return;
        }

        union_node << clone(type);
      };

      for (auto* target : targets)
      {
        if (!target || !target->reification)
          continue;

        auto params = target->reification / Params;
        if (params->empty())
          continue;

        add_unique(params->front() / Type);
      }

      if (union_node->empty())
        return {};

      if (union_node->size() == 1)
        return clone(union_node->front());

      return union_node;
    }

    bool refine_receiver_type(
      const Node& func,
      const Location& recv_loc,
      Node& current_type,
      const std::vector<Reification*>& targets)
    {
      auto refined = receiver_type_from_targets(targets);

      if (!refined || current_type->equals(refined))
        return false;

      current_type = clone(refined);

      for (auto& param : *(func / Params))
      {
        if ((param / LocalId)->location() != recv_loc)
          continue;

        auto current = param / Type;
        if (!current->equals(refined))
          param->replace(current, clone(refined));
        break;
      }

      return true;
    }

    bool receiver_is_param(const Node& func, const Location& recv_loc) const
    {
      for (auto& param : *(func / Params))
      {
        if ((param / LocalId)->location() == recv_loc)
          return true;
      }

      return false;
    }

    bool refine_function_params(
      Reification& target, const Node& args, bool behavior_args)
    {
      if (!target.reification)
        return false;

      auto params = target.reification / Params;
      auto def_params = target.def / Params;

      if (params->size() != args->size())
        return false;

      bool changed = false;

      auto find_create_field_def = [&](const Node& def_param) -> Node {
        auto parent_cls = target.def->parent(ClassDef);
        if (!parent_cls)
          return {};
        if ((target.def / Ident)->location().view() != "create")
          return {};

        auto field_name = (def_param / Ident)->location().view();
        for (auto& child : *(parent_cls / ClassBody))
        {
          if (child != FieldDef)
            continue;
          if ((child / Ident)->location().view() != field_name)
            continue;
          return child;
        }

        return {};
      };

      auto sync_create_field_type =
        [&](const Node& def_param, const Node& refined_type) {
          auto parent_cls = target.def->parent(ClassDef);
          if (!parent_cls)
            return false;
          if ((target.def / Ident)->location().view() != "create")
            return false;

          auto class_id = target.reification / Type;
          if (class_id != ClassId)
            return false;

          auto* class_reif = find_class_reification(class_id);
          if (!class_reif || !class_reif->reification)
            return false;

          auto field_name = (def_param / Ident)->location().view();
          auto field_def = find_create_field_def(def_param);

          if (
            !field_def ||
            !(contains_typeparam_ref(field_def / Type) ||
              has_unresolved_type(field_def / Type, class_reif->subst)))
            return false;

          auto fields = class_reif->reification / Fields;
          for (auto& field : *fields)
          {
            if ((field / FieldId)->location().view() != field_name)
              continue;

            auto current_field = field / Type;
            auto refined = clone(refined_type);

            if (current_field->equals(refined))
              return false;

            field->replace(current_field, refined);
            return true;
          }

          return false;
        };

      for (size_t i = 0; i < args->size(); i++)
      {
        auto actual = actual_arg_type(args->at(i), behavior_args);

        if (!actual)
          continue;

        auto param = params->at(i);
        auto def_param = def_params->at(i);
        auto current = param / Type;
        bool generic_origin = contains_typeparam_ref(def_param / Type);
        auto unresolved_seed = reify_type(def_param / Type, target.subst);
        bool is_create = (target.def / Ident)->location().view() == "create";
        auto field_def = find_create_field_def(def_param);
        bool generic_create_field = field_def &&
          (contains_typeparam_ref(field_def / Type) ||
           has_unresolved_type(field_def / Type, target.subst));
        bool constructor_seed = is_create && generic_create_field &&
          contains_typeid(current) && !contains_typeid(actual) &&
          vbcc::IRSubtype(top, actual, current);
        // The "seed" form is the param's reified declared type — what a fresh
        // reification of the def's type produces with the current subst.
        // We only want to *replace* the seed when it represents an
        // unresolved type (a Shape TypeId, Dyn, or a Union that still has
        // unresolved members). A Union of fully-concrete types is a valid
        // declared type and should be merged with — not replaced by —
        // call-site arguments. Otherwise a single call site passing the
        // narrower side of `T | none` (e.g. `none`) would silently drop
        // the wider member from the function's param type.
        bool seed_unresolved = current->in({TypeId, Dyn}) ||
          ((current == Union) &&
           (contains_typeid(current) || contains_dyn(current)));
        bool replacing_seed = unresolved_seed &&
          current->equals(unresolved_seed) && seed_unresolved;

        // A TypeId that was resolved from a class-level TypeParam is a valid
        // concrete type. Don't replace it — the class's subst already
        // determined the correct type. Method-level TypeParams should still
        // be refined from call-site argument types.
        bool shape_param = false;
        if (replacing_seed && (current == TypeId) && generic_origin)
        {
          auto def_type = def_param / Type;
          auto tp_name = (def_type == Type) ? def_type->front() : def_type;

          if (tp_name == TypeName)
          {
            auto last_elem = tp_name->back();
            auto tp_ident = last_elem / Ident;
            auto parent_cls = target.def->parent(ClassDef);

            if (parent_cls)
            {
              for (auto& tp : *(parent_cls / TypeParams))
              {
                if ((tp / Ident)->location() == tp_ident->location())
                {
                  replacing_seed = false;
                  break;
                }
              }
            }

            // A TypeId resolved from a Shape ClassDef is a structural type
            // that accepts any implementing class. Refining it to a single
            // call-site's actual class would prevent later calls with
            // different implementing classes from typechecking. Skip
            // refinement entirely for shape parameters with at least one
            // implementor: resolve_shapes already expanded the TypeId to a
            // Union of implementors. For unresolvable shapes (empty
            // implementor set), fall through to the normal refinement path
            // so the caller's actual type provides a usable parameter type.
            if (replacing_seed)
            {
              auto def = find_def(top, tp_name);
              if (def && (def == ClassDef) && ((def / Shape) == Shape))
              {
                // Determine whether the shape has any implementors. After
                // resolve_shapes/prune_empty_shape_unions, the shape's
                // reification body is one of:
                //   - Dyn: pruned-empty (no implementors)
                //   - Union (empty): not pruned yet, no implementors
                //   - Union (1+ entries) or single ClassId/TypeId: implementors
                bool empty_shape = true;
                for (auto& key : map_order)
                {
                  for (auto& cr : map[key])
                  {
                    if (!cr.id || !same_reification_id(cr.id, current))
                      continue;
                    if (cr.reification && (cr.reification == Type))
                    {
                      auto& body = cr.reification->back();
                      if (body == Dyn)
                        empty_shape = true;
                      else if (body == Union)
                        empty_shape = body->empty();
                      else
                        empty_shape = false;
                    }
                    else
                    {
                      // Pending reification — assume it will get implementors.
                      empty_shape = false;
                    }
                    goto found;
                  }
                }
              found:
                if (!empty_shape)
                {
                  replacing_seed = false;
                  shape_param = true;
                }
              }
            }
          }
        }

        if (shape_param)
          continue;

        if (
          !generic_origin &&
          !has_unresolved_type(def_param / Type, target.subst) &&
          !constructor_seed)
          continue;

        Node merged =
          (contains_dyn(current) || replacing_seed || constructor_seed) ?
          clone(actual) :
          merge_refined_type(current, actual);

        if (!current->equals(merged))
        {
          param->replace(current, merged);
          changed = true;
        }

        changed |= sync_create_field_type(def_param, merged);
      }

      return changed;
    }

    Node find_method_return_type(const std::vector<Reification*>& targets)
    {
      Nodes ret_types;

      for (auto* target : targets)
      {
        if (!target || !target->reification)
          continue;

        Node ret = target->reification / Type;

        if (!ret)
          continue;

        bool dup = false;

        for (auto& existing : ret_types)
        {
          if (existing->equals(ret))
          {
            dup = true;
            break;
          }
        }

        if (!dup)
          ret_types.push_back(ret);
      }

      if (ret_types.empty())
        return {};

      if (ret_types.size() == 1)
        return clone(ret_types.front());

      Node union_node = Union;

      for (auto& ret : ret_types)
        union_node << clone(ret);

      return union_node;
    }

    // Given a reified receiver type (possibly a union) and a MethodId string,
    // find the method's function return type by searching matching class
    // reifications. If multiple receivers contribute distinct return types,
    // return their union.
    Node find_method_return_type(Node recv_type, const std::string& method_id)
    {
      auto recv_set = extract_receivers(recv_type);
      Nodes ret_types;

      // Use index-based loop because find_func_return_type below may
      // recursively push to map_order via reify_emitted_type ->
      // find_or_push, invalidating range-for iterators.
      for (size_t ki = 0; ki < map_order.size(); ki++)
      {
        Node key = map_order[ki];

        if (key != ClassDef)
          continue;

        // Inner loop is also index-based: find_or_push can also push to
        // map[key] (the per-key Reification vector), invalidating refs.
        auto& r_vec_initial = map[key];

        for (size_t ri = 0; ri < r_vec_initial.size(); ri++)
        {
          // Re-resolve the vector each iteration in case map_order's
          // backing buffer was reallocated since the last iteration. The
          // inner Reification vector itself can also have grown, so
          // bound-check against its current size.
          auto& r_vec = map[map_order[ki]];
          if (ri >= r_vec.size())
            break;
          auto& r = r_vec[ri];

          if (!r.id)
            continue;

          if ((r.def / Shape) == Shape)
            continue;

          bool matches = recv_set.all;

          for (auto& recv : recv_set.types)
          {
            if (same_reification_id(r.id, recv))
            {
              matches = true;
              break;
            }
          }

          if (!matches)
            continue;

          Node ret;

          if (r.reification)
          {
            auto methods = r.reification / Methods;

            for (auto& m : *methods)
            {
              if ((m / MethodId)->location().view() == method_id)
              {
                ret = find_func_return_type(m / FunctionId);
                break;
              }
            }
          }
          else
          {
            // Pending class — its r.reification hasn't been built yet
            // (drain_worklist hasn't reached it). Resolve the method via
            // method_invocations: for any MI that targets this class with
            // the right method_id, find the matching Function in the
            // class's ClassBody, build the func subst (mirroring
            // register_method), and look up the return type via
            // find_func_return_type. This avoids the Dyn-fallback that
            // would otherwise leak into IR (Dyn-rule, see AGENTS.md).
            for (auto& mi : method_invocations)
            {
              if (mi.method_id != method_id)
                continue;

              if (!mi_targets(mi, r.id))
                continue;

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

                NodeMap<Node> combined = mi.call_subst;

                for (auto& [k, v] : r.subst)
                  combined.insert_or_assign(k, v);

                NodeMap<Node> func_subst = r.subst;

                for (size_t i = 0; i < func_tps->size(); i++)
                {
                  auto ta = mi.typeargs->at(i);

                  if (ta == Type)
                    func_subst[func_tps->at(i)] = reify_type(ta, combined);
                }

                // Compute the return type directly without queueing the
                // function for reification — find_method_return_type is
                // a query, not a registration. Return type is wfType,
                // built by reify_emitted_type from the function's
                // declared return type and the call subst.
                //
                // BDGI: skip if def_type or its substitution contains
                // unresolved formals. Stage 2 (reify_function_stage2)
                // will re-resolve once cross-function constraint flow
                // binds the relevant α's. Avoids leaking TypeVar into
                // reify_type's IR-emission path during stage 1.
                auto def_type = f / Type;

                if (
                  def_type->front() != TypeVar &&
                  !has_unresolved_type(def_type, func_subst))
                  ret = reify_emitted_type(
                    def_type, func_subst, f / Ident, "return type");

                break;
              }

              if (ret)
                break;
            }
          }

          if (!ret)
            continue;

          bool dup = false;

          for (auto& existing : ret_types)
          {
            if (existing->equals(ret))
            {
              dup = true;
              break;
            }
          }

          if (!dup)
            ret_types.push_back(ret);
        }
      }

      if (ret_types.empty())
        return {};

      if (ret_types.size() == 1)
        return clone(ret_types.front());

      Node union_node = Union;

      for (auto& ret : ret_types)
        union_node << clone(ret);

      return union_node;
    }

    // Phase 6 stage-2: post-block re-solve. After block_on_all on
    // this reification's direct callees has unblocked, re-query the
    // constraint solver for any formal_typevar that was unsolved in
    // stage 1. If newly bound, update r.subst and propagate into the
    // reified IR (Type, Params, Vars, Body Args). Per-Reification
    // identities are stored in `reify_formal_alphas` (TypeParam →
    // α_id, indexed by Reification*).
    void reify_function_stage2(Reification& r);

    bool reify_function(Reification& r)
    {
      // Clear per-function local type tracking.
      local_types.clear();
      lookup_info.clear();
      pinned_locals.clear();
      // typevar_store accumulates constraints ACROSS reifications.
      // Per Phase 5 cross-functional propagation: when reifying a
      // caller, its own body-walk constraints share the store with
      // any callee reifications (e.g., a lifted lambda's apply
      // body). Each formal gets a fresh α_k (unique Location), so
      // identities don't collide. Constraints are monotone (only
      // added, never removed), so cross-reification accumulation is
      // safe.

      // Reify the function signature.
      auto def_type = r.def / Type;
      bool typevar_return = (def_type->front() == TypeVar);

      // Phase 3b.4 body-driven binding: detect unbound formals (declared
      // TypeParams without an entry in r.subst). If the return type
      // references any unbound formal, defer r_type emission until after
      // body walk + evidence gathering. The reverted Phase 0.5 / current
      // partial-binding fills constrained slots from arg types and leaves
      // unconstrained formals out of r.subst.
      auto def_tps = r.def / TypeParams;
      Nodes unbound_formals;
      // Phase 6: for each unbound formal, intern a constraint-store
      // identity (the TypeParam's FQ TypeName). Used by the solver to
      // collect observations about the formal during constraint
      // emission; the solver result fills r.subst.
      //
      // The seed value placed in r.subst[tp] is a TypeVar leaf with
      // a fresh unique Location — a placeholder so resolve_typearg
      // returns it instead of erroring. We intern BOTH the seed
      // TypeVar leaf AND the FQ TypeName, then `unify` them so they
      // share the same α_id. This way:
      //  - Body emission via TypeName(T) → builds FQ → interns →
      //    α_id (root).
      //  - Cross-reify gather, working with TypeArgs after
      //    apply_subst, sees the seed TypeVar leaf — interns →
      //    α_id (same root after unify).
      // formal_typevars maps TypeParam → FQ TypeName so post-walk
      // solver consumption can look up the α for each formal.
      NodeMap<Node> formal_typevars;
      for (auto& tp : *def_tps)
      {
        if (r.subst.find(tp) == r.subst.end())
        {
          unbound_formals.push_back(tp);
          auto fq = fq_typeparam(scope_path(tp), tp);
          formal_typevars[tp] = fq;
          auto fq_id = typevar_store.intern(fq);
          // Seed: fresh TypeVar leaf, then unify its identity with
          // the FQ identity so cross-reify gather and direct subtype
          // emission share the same α root.
          auto seed_tv = make_typevar();
          auto seed_id = typevar_store.intern(seed_tv);
          typevar_store.unify(fq_id, seed_id);
          r.subst[tp] = Type << seed_tv;
        }
      }

      // Persist formal_typevars on the Reification for stage 2 of
      // the two-stage NodeWorker pipeline. After block_on_all on
      // direct callees, stage 2 re-solves these α and propagates
      // any newly-bound formals into the IR.
      if (!formal_typevars.empty())
      {
        auto& alpha_vec = reify_formal_alphas[&r];
        alpha_vec.clear();
        for (auto& [tp, fq] : formal_typevars)
          alpha_vec.emplace_back(tp, typevar_store.intern(fq));
      }

      // Phase B2: emit arg-vs-formal Subtype constraints for every
      // Call/CallDyn in the body, using source types and r.subst.
      // Fires for generic callers where some formal is α_k (seeded
      // above); emission decomposes through shape match etc. and
      // records the constraints into typevar_store. The solver
      // consumption block below then queries store.solve(α_k) and
      // fills r.subst gaps from solved bindings.
      // Cross-function flow: also fire emit when r.subst contains
      // TypeVar values inherited from an enclosing caller's seed
      // (e.g. reduce::2::1 reified for sum's call has T_reduce =
      // sum::T's seed_TV). Such values must NOT block emission —
      // reduce's body emissions reach sum::T's α via the unify
      // chain, providing the concrete binding (e.g. via vector::
      // each's i32 element type).
      bool has_typevar_subst = false;
      for (auto& [tp, val] : r.subst)
      {
        if (val && val == Type && !val->empty() && val->front() == TypeVar)
        {
          has_typevar_subst = true;
          break;
        }
      }

      if (!unbound_formals.empty() || has_typevar_subst)
      {
        emit_source_call_constraints(r.def, r.subst);

        // Phase 6 early solver consumption: now that constraints
        // have been emitted into the global store, query for solved
        // bindings BEFORE the body walk. This way lambda New/Stack
        // sites and other reifications triggered during body walk
        // pick up the concrete bindings rather than the seed.
        for (auto& [tp, fq] : formal_typevars)
        {
          auto alpha_id = typevar_store.intern(fq);
          auto solved = typevar_store.solve(alpha_id);
          if (!solved || (solved == Union && solved->empty()))
            continue;
          // Skip if solved still contains free TypeVars — incomplete.
          bool has_tv = false;
          solved->traverse([&](const Node& n) {
            if (n == TypeVar)
              has_tv = true;
            return !has_tv;
          });
          if (has_tv)
            continue;
          // Overwrite the seed (Type wrapping the FQ TypeName for tp)
          // with the solved binding. If r.subst no longer has tp at
          // all, that's unexpected (we seeded it above) but write
          // through anyway. If cur is something other than the seed,
          // another mechanism already bound it; preserve.
          auto it = r.subst.find(tp);
          if (it == r.subst.end())
          {
            r.subst[tp] = Type << solved;
            continue;
          }
          auto cur = it->second;
          // Overwrite the seed (Type wrapping a TypeVar leaf) with
          // the solved binding. If cur isn't the seed, another
          // mechanism wrote a concrete value; preserve it.
          if (
            cur && cur == Type && !cur->empty() &&
            cur->front() == TypeVar)
            r.subst[tp] = Type << solved;
        }
      }

      bool body_driven = !typevar_return && !unbound_formals.empty() &&
        has_unresolved_type(def_type, r.subst);

      Node r_type;

      if (typevar_return || body_driven)
        r_type = {}; // Will be inferred from Return terminals after body.
      else
        r_type =
          reify_emitted_type(def_type, r.subst, r.def / Ident, "return type");
      Node params = Params;

      for (auto& p : *(r.def / Params))
      {
        auto p_type = reify_type(p / Type, r.subst);
        local_types[(p / Ident)->location()] = p_type;
        params << (Param << (LocalId ^ (p / Ident)) << p_type);
      }

      // Reify the function body.
      Node vars = Vars;
      std::vector<Location> var_locs;
      Node labels = clone(r.def / Labels);

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
            local_types[(n / LocalId)->location()] = Array << U8;
          }
          else if (n->in({Eq, Ne, Lt, Le, Gt, Ge}))
          {
            reify_primitive(Bool);
            local_types[(n / LocalId)->location()] = Bool;
          }
          else if (n->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
          {
            reify_primitive(F64);
            local_types[(n / LocalId)->location()] = F64;
          }
          else if (n == Bits)
          {
            reify_primitive(U64);
            local_types[(n / LocalId)->location()] = U64;
          }
          else if (n == Len)
          {
            reify_primitive(USize);
            local_types[(n / LocalId)->location()] = USize;
          }
          else if (n == MakePtr)
          {
            reify_primitive(Ptr);
            local_types[(n / LocalId)->location()] = Ptr;
          }
          else if (n == MakeCallback)
          {
            reify_primitive(Ptr);
            local_types[(n / LocalId)->location()] = Ptr;

            // Find the lambda's type and register its @callback method.
            // First check local_types (works when source was from New or from a
            // typed function wrapper that may need deferred shape resolution).
            auto src_loc = (n / Rhs)->location();
            auto src_it = local_types.find(src_loc);
            Node callback_type;

            if (src_it != local_types.end())
              callback_type = clone(src_it->second);

            if (!callback_type)
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
                  callback_type = clone(stmt / ClassId);
                }
                else if (
                  (stmt == Call) && ((stmt / LocalId)->location() == trace_loc))
                {
                  // Find the Function reification for this Call, then
                  // trigger reification of its enclosing ClassDef.
                  auto funcid_loc = (stmt / FunctionId)->location().view();
                  NodeMap<Node> class_subst;

                  for (auto& key : map_order)
                  {
                    if (key != Function)
                      continue;

                    for (auto& reif : map[key])
                    {
                      if (reif.id && (reif.id->location().view() == funcid_loc))
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

                  // Trigger class reification AFTER the map_order loop
                  // to avoid iterator invalidation (find_or_push may
                  // append to map_order).
                  if (call_enc)
                    callback_type =
                      find_or_push(call_enc, std::move(class_subst));
                }
              }
            }

            if (callback_type)
              reify_make_callback(n, callback_type);
            else
              n->parent()->replace(
                n, err(n, "make_callback: cannot determine lambda type"));
          }
          else if (n == CodePtrCallback)
          {
            reify_primitive(Ptr);
            local_types[(n / LocalId)->location()] = Ptr;
          }
          else if (n->in({
                     FreeCallback,
                     Pin,
                     Unpin,
                     Merge,
                     AddExternal,
                     RemoveExternal,
                   }))
          {
            reify_primitive(None);
            local_types[(n / LocalId)->location()] = None;
          }
          else if (n == Freeze)
          {
            // Propagate type from source to destination.
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end())
              local_types[(n / LocalId)->location()] = clone(src_it->second);
          }
          else if (n == Cttz)
          {
            // Propagate type from source to destination (same-type unop).
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end())
              local_types[(n / LocalId)->location()] = clone(src_it->second);
          }
          else if (n->in({ArrayCopy, ArrayFill}))
          {
            reify_primitive(None);
            local_types[(n / LocalId)->location()] = None;
          }
          else if (n == ArrayCompare)
          {
            reify_primitive(I64);
            local_types[(n / LocalId)->location()] = I64;
          }
          else if (n->in({Copy, Move}))
          {
            // Propagate type from source to destination.
            auto src_loc = (n / Rhs)->location();
            auto dst_loc = (n / LocalId)->location();
            auto src_it = local_types.find(src_loc);

            // Pinned destinations keep their declared type (from a
            // concrete TypeAssertion). Don't let Copy/Move narrow.
            if (src_it != local_types.end() && !pinned_locals.count(dst_loc))
              local_types[dst_loc] = clone(src_it->second);
          }
          else if (n == RegisterRef)
          {
            // RegisterRef: dst = &src. Result type is Ref << type(src).
            auto src_it = local_types.find((n / Rhs)->location());

            if (src_it != local_types.end())
            {
              ensure_ref_reified(src_it->second);
              local_types[(n / LocalId)->location()] = Ref
                << clone(src_it->second);
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
                local_types[(n / LocalId)->location()] = Ref << ft;
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
                local_types[(n / LocalId)->location()] = Ref << elem;
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
                local_types[(n / LocalId)->location()] = Ref << elem;
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

            if (src_it != local_types.end() && (src_it->second == Ref) &&
                !src_it->second->empty())
            {
              auto payload = src_it->second->front();
              local_types[(n / LocalId)->location()] = clone(payload);

              // Phase 6 constraint emission: the stored value's type
              // contributes a lower bound on TypeVars in the ref's
              // payload. For a direct TypeVar payload, the value
              // becomes a lower bound. For a Union payload, the value
              // must subtype some arm: if it matches a concrete arm
              // structurally, no TypeVar constraint; otherwise it's
              // attributed to the (single) TypeVar arm if any.
              auto val_loc = ((n / Arg) / Rhs)->location();
              auto val_it = local_types.find(val_loc);
              if (val_it != local_types.end() && val_it->second)
              {
                auto val_type = val_it->second;

                if (payload == TypeVar)
                {
                  auto alpha_id =
                    typevar_store.intern(payload);
                  typevar_store.add_lower(alpha_id, val_type);
                }
                else if (payload == Union)
                {
                  // Heuristic: if val_type matches any concrete arm
                  // structurally, emit no constraint (val flows into
                  // that arm). Else, attribute to the single TypeVar
                  // arm if exactly one exists.
                  Node tv_arm;
                  bool matched_concrete = false;
                  for (auto& arm : *payload)
                  {
                    if (arm == TypeVar)
                    {
                      if (!tv_arm)
                        tv_arm = arm;
                      else
                        tv_arm = {}; // multiple TypeVars: ambiguous
                    }
                    else if (val_type && val_type->equals(arm))
                    {
                      matched_concrete = true;
                      break;
                    }
                  }
                  if (!matched_concrete && tv_arm)
                  {
                    auto alpha_id =
                      typevar_store.intern(tv_arm);
                    typevar_store.add_lower(alpha_id, val_type);
                  }
                }
              }
            }
          }
          else if (n == Var)
          {
            var_locs.push_back((n / Ident)->location());
            remove.push_back(n);
          }
          else if (n == TypeAssertion)
          {
            auto loc = (n / LocalId)->location();
            // Concrete TypeAssertion: pin local_types[loc] to the
            // declared type. The user's annotation is the source of
            // truth; subsequent Copy/Move into this local must not
            // narrow it (e.g., `var best: i32 | none = none` must not
            // collapse to `none`). Skip TypeAssertions whose type
            // contains TypeVars (sugar may emit capture-ref placeholders)
            // or unbound formals (bare-U assertions handled by BDB above).
            // has_unresolved_type covers both: TypeVar leaves and
            // TypeParam refs not in r.subst.
            //
            // Phase 6: when the assertion type references unbound
            // formals (e.g., `var best: U | none`), build an augmented
            // subst that maps each unbound formal to its α_k TypeVar
            // and pin local_types[loc] to that. This makes
            // RegisterRef pick up Ref<<Union(α_U, none) and the Store
            // emission sees the TypeVar in the payload.
            auto assert_type = n / Type;
            if (!has_unresolved_type(assert_type, r.subst))
            {
              auto reified = reify_type(assert_type, r.subst);
              if (reified && reified != Dyn)
              {
                local_types[loc] = reified;
                pinned_locals.insert(loc);
              }
            }
            // If has_unresolved_type is true, the assertion type
            // references an unbound formal. r.subst already has seed
            // TypeVar leaves for unbound formals (unified with the
            // FQ identity in the constraint store), so reify_type
            // would substitute the seed TypeVar in. We don't pin
            // here — the seed TypeVar isn't suitable as a local_type
            // pin, and the constraint solver handles binding.
            remove.push_back(n);
          }
          else if (n->in({New, Stack}))
          {
            // Save the type before reify_new transforms the node.
            auto orig_type = n / Type;
            auto new_type = reify_emitted_type(
              orig_type, r.subst, n / Type, "constructed type");

            gather_lambda_apply_constraints(orig_type, r.subst);

            reify_new(n, r.subst);
            // After reify_new, dst is first child.
            local_types[(n / LocalId)->location()] = new_type;
          }
          else if (n == Lookup)
          {
            // Save receiver location before reify_lookup transforms the node.
            auto recv_loc = (n / Rhs)->location();
            reify_lookup(n, r.subst);
            // After reify_lookup: Lookup << dst << src << MethodId.
            auto mid = (n / MethodId)->location().view();
            lookup_info[(n / LocalId)->location()] = {
              std::string(mid), recv_loc};
          }
          else if (n == Call)
          {
            reify_call(n, r.subst);
            // Track Call return type from the function reification.
            auto ret = find_func_return_type(n / FunctionId);
            if (ret)
              local_types[(n / LocalId)->location()] = ret;
          }
          else if (n->in({CallDyn, TryCallDyn}))
          {
            // Track CallDyn return type by resolving the method on
            // the receiver's reified class.
            auto src_loc = (n / Rhs)->location();
            auto dst_loc = (n / LocalId)->location();
            auto li = lookup_info.find(src_loc);
            if (li != lookup_info.end())
            {
              auto recv_it = local_types.find(li->second.recv_loc);
              if (recv_it != local_types.end())
              {
                auto ret = find_method_return_type(
                  recv_it->second, li->second.method_id);

                // Structural fallback for ref deref: ref[X].*(ref[X])
                // returns X. This handles cases where ensure_ref_reified
                // created the ref wrapper without proper subst for method
                // registration.
                if (!ret && recv_it->second == Ref)
                {
                  auto& mid = li->second.method_id;
                  if (mid.starts_with("*::"))
                    ret = clone(recv_it->second->front());
                }

                if (ret)
                  local_types[dst_loc] = ret;
              }
            }
          }
          else if (n == NewArray)
          {
            auto arr_type =
              Array << reify_emitted_type(
                n / Type, r.subst, n / Type, "array element type");
            local_types[(n / LocalId)->location()] = clone(arr_type);
            n / Type = arr_type;
          }
          else if (n == NewArrayConst)
          {
            auto inner = reify_emitted_type(
              n / Type, r.subst, n / Type, "array element type");

            // Distinguish tuples from array literals via the LocalId
            // prefix that anf assigned. A tuple is encoded as a
            // NewArrayConst with l_local prefix; an array literal
            // (including arrays whose element type happens to be a
            // tuple) uses the l_arraylit prefix. Without this
            // distinction, an array-of-tuples would collapse to a
            // bare TupleType.
            auto loc_view = (n / LocalId)->location().view();
            bool is_array_lit =
              loc_view.size() >= 5 && loc_view.substr(0, 5) == "array";

            if (!is_array_lit && inner == TupleType)
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
              if (is_array_lit)
                ensure_array_reified(orig_type, r.subst);
            }
          }
          else if (n == FFI)
          {
            reify_ffi(n, r);
          }
          else if (n == FFIStruct)
          {
            auto layout_type = reify_emitted_type(
              n / Type, r.subst, n / Type, "FFI layout type");
            auto result_type = reify_emitted_type(
              ffi_struct_result_type(),
              r.subst,
              n / Type,
              "FFI struct result type");
            n / Type = layout_type;
            local_types[(n / LocalId)->location()] = clone(result_type);
          }
          else if (n == FFILoad)
          {
            auto field_type =
              reify_emitted_type(n / Type, r.subst, n / Type, "FFI field type");
            n / Type = field_type;
            local_types[(n / LocalId)->location()] = clone(field_type);
          }
          else if (n == FFIStore)
          {
            auto field_type =
              reify_emitted_type(n / Type, r.subst, n / Type, "FFI field type");
            reify_primitive(None);
            n / Type = field_type;
            local_types[(n / LocalId)->location()] = None;
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
                << clone(n / LocalId) << clone(n / Arg)
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
                Node replacement = Const << (LocalId ^ dst_loc) << clone(None)
                                         << clone(None);

                body->replace(n, replacement);
              }
              else if (remaining == 1)
              {
                // One element: ArrayRefConst + Load.
                auto ref_loc = top->fresh(Location("splat"));

                Node aref = ArrayRefConst << (LocalId ^ ref_loc)
                                          << clone(n / Arg)
                                          << (Int ^ std::to_string(before));

                Node load = Load << (LocalId ^ dst_loc) << (LocalId ^ ref_loc);

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
                  NewArrayConst << (LocalId ^ dst_loc) << clone(ttype)
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
                    ArrayRefConst << (LocalId ^ src_ref) << clone(n / Arg)
                                  << (Int ^ std::to_string(before + i)));

                  // Load source value.
                  replacements.push_back(
                    Load << (LocalId ^ val_loc) << (LocalId ^ src_ref));

                  // Get ref to destination element.
                  replacements.push_back(
                    ArrayRefConst << (LocalId ^ dst_ref)
                                  << (Arg << ArgCopy << (LocalId ^ dst_loc))
                                  << (Int ^ std::to_string(i)));

                  // Store value into destination.
                  replacements.push_back(
                    Store << (LocalId ^ old_val) << (LocalId ^ dst_ref)
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

      // Infer TypeVar return type from Return terminals after body
      // processing. By now, local_types has been populated for all
      // statements in the body. Collect all distinct return types and
      // build a union if there are multiple.
      if (typevar_return)
      {
        Nodes ret_types;

        for (auto& l : *labels)
        {
          auto term = l / Return;

          if (term != Return)
            continue;

          auto ret_loc = (term / LocalId)->location();
          auto it = local_types.find(ret_loc);

          if (it == local_types.end())
            continue;

          // Check if this type is already covered.
          bool dup = false;

          for (auto& existing : ret_types)
          {
            if (existing->equals(it->second))
            {
              dup = true;
              break;
            }
          }

          if (!dup)
            ret_types.push_back(clone(it->second));
        }

        if (ret_types.size() == 1)
        {
          r_type = ret_types.front();
        }
        else if (ret_types.size() > 1)
        {
          Node union_node = Union;

          for (auto& rt : ret_types)
            union_node << clone(rt);

          r_type = union_node;
        }

        // If we still don't have a type, check if all exits are
        // Raise/Jump — the function never returns normally.
        if (!r_type)
        {
          bool all_nonlocal = true;

          for (auto& l : *labels)
          {
            auto term = l / Return;

            if (term->in({Jump, Cond}))
              continue;

            if (term != Raise)
            {
              all_nonlocal = false;
              break;
            }
          }

          if (all_nonlocal)
          {
            reify_primitive(clone(None));
            r_type = clone(None);
          }
        }

        // Last resort: mark as TypeVar (intermediate marker) and let the
        // deferred-typevar second pass try to refine it once all callees
        // are reified. The post-worklist check at line ~787 emits an
        // unresolved-return error if r_type is still TypeVar after all
        // refinement attempts. Only reached for typevar_return functions
        // (lambdas) where the first body walk couldn't determine the
        // return type. TypeVar (not Dyn) is the principled marker per
        // AGENTS.md: Dyn is reserved for the IR encoding of `any`.
        if (!r_type)
          r_type = make_typevar();
      }

      // Phase 6 — solver consumption (NEW, runs alongside legacy
      // body-driven binding for now): for each unbound formal,
      // query the constraint store for a binding. If solved to a
      // non-bottom type, record it for r.subst update below. The
      // legacy var/return-evidence mechanism still runs for cases
      // the solver can't yet handle (e.g., return type evidence
      // that doesn't flow through Stores).
      NodeMap<Node> typevar_solved_formals;
      for (auto& [tp, fq] : formal_typevars)
      {
        auto alpha_id = typevar_store.intern(fq);
        auto solved = typevar_store.solve(alpha_id);
        // Bottom (empty Union with no children) means no observation.
        // Skip; legacy mechanism may still bind from return evidence.
        if (!solved || (solved == Union && solved->empty()))
          continue;
        // Skip if solved still contains free TypeVars — incomplete.
        bool has_tv = false;
        solved->traverse([&](const Node& n) {
          if (n == TypeVar)
            has_tv = true;
          return !has_tv;
        });
        if (has_tv)
          continue;
        typevar_solved_formals[tp] = Type << solved;
      }

      // Phase 6: post-walk body-driven binding for unbound formals.
      // The brittle Phase 3b mechanisms (var-evidence, return-evidence,
      // refused_formals via U-mention scan, tainted_locals) are
      // SUBSUMED by the constraint solver:
      //  - call-arg variance: emitted by emit_source_call_constraints
      //    at every Call/CallDyn site (handles each-pattern bindings
      //    where U flows via shape-match on lambda apply params).
      //  - return-arg variance: emitted by the same helper at every
      //    label whose terminator is a Return statement.
      //  - cross-reify gather: gather_lambda_apply_constraints walks
      //    lifted-lambda apply bodies emitting Store constraints.
      //  - per-Reification α_k seeding: r.subst[U_k] = Type<TypeVar α_k>
      //    at the TypeParam's Ident Location ensures the constraints
      //    reach the correct identity.
      // Solver consumption queries store.solve(α_k) and overwrites the
      // α_k seed in r.subst with the solved binding.
      if (body_driven)
      {
        for (auto& [tp, ty] : typevar_solved_formals)
        {
          auto it = r.subst.find(tp);
          if (it == r.subst.end())
          {
            r.subst[tp] = ty;
            continue;
          }
          // If the existing entry is the seed (Type wrapping a
          // TypeVar leaf), overwrite with the solved binding.
          // Otherwise an earlier mechanism wrote a concrete value;
          // preserve it.
          auto cur = it->second;
          if (cur && cur == Type && !cur->empty() && cur->front() == TypeVar)
            r.subst[tp] = ty;
        }

        // Re-emit r_type with the (possibly-updated) substitution. If a
        // formal is still unbound, reify_emitted_type emits a clean
        // "type parameter cannot be inferred" error.
        //
        // BDGI: skip if def_type still references unresolved formals
        // after subst. Cross-function constraint flow may not have
        // propagated yet at this stage 1 finalization point — stage 2
        // (reify_function_stage2) will re-emit when callees' constraints
        // are visible. Premature reification would leak TypeVar α into
        // reify_type's IR boundary.
        if (!has_unresolved_type(def_type, r.subst))
          r_type =
            reify_emitted_type(def_type, r.subst, r.def / Ident, "return type");
        else if (!r_type)
          // Placeholder — stage 2 will overwrite it after solver
          // consumption binds the relevant α's.
          r_type = make_typevar();
      }

      // Implicit none return: when a function returns none, append a
      // none constant and use it as the return value for any Return
      // terminator. This lets users omit the trailing `none` in
      // functions and lambdas that return none.
      if (r_type->type() == None)
      {
        for (auto& l : *labels)
        {
          auto term = l / Return;

          if (term != Return)
            continue;

          auto none_loc = top->fresh(Location("none"));
          auto body = l / Body;
          body << (Const << (LocalId ^ none_loc) << None << None);
          term->replace(term->front(), LocalId ^ none_loc);
        }
      }

      // Build VarDef nodes with types from local_types.
      // For vars whose type couldn't be tracked in this first-pass body
      // walk (typically: a CallDyn whose receiver class is a not-yet-
      // reified lambda with TypeVar return), emit `TypeVar` as a clear
      // intermediate marker. The second pass refines from label_exits
      // and replaces TypeVar with the concrete aggregate type. A
      // post-pass scan converts any remaining TypeVar to Dyn + emits
      // an error so wfType validates.
      for (auto& loc : var_locs)
      {
        auto it = local_types.find(loc);
        Node var_type = (it != local_types.end()) ? clone(it->second) : make_typevar();
        vars << (VarDef << (LocalId ^ loc) << var_type);
      }

      if ((r.def / Lhs) == Once)
      {
        r.reification = FuncOnce << r.id << params << r_type << vars << labels;
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

          // Check local_types first (works for New/Stack and typed function
          // wrappers that resolve to a concrete lambda class).
          auto it = local_types.find(ret_loc);

          if (it != local_types.end())
          {
            if (register_callback_type(it->second))
            {
              break;
            }
          }

          // Check if the return value comes from a Call (e.g., create).
          for (auto& stmt : *body_node)
          {
            if ((stmt == Call) && ((stmt / LocalId)->location() == ret_loc))
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
                  if (reif.id && (reif.id->location().view() == funcid_loc))
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
                register_callback_type(class_id);
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

      // Already-reified IR type. Return a clone so the caller can safely
      // insert it into a new part of the AST.
      if (type == Dyn)
        return clone(type);

      if (type->in(
            {ClassId,
             TypeId,
             None,
             Bool,
             I8,
             I16,
             I32,
             I64,
             U8,
             U16,
             U32,
             U64,
             ILong,
             ULong,
             ISize,
             USize,
             F32,
             F64,
             Ptr}))
      {
        return clone(type);
      }

      if (type == Array)
        return Array << reify_type(type->front(), subst);

      if (type == Ref)
        return Ref << reify_type(type->front(), subst);

      if (type == Cown)
        return Cown << reify_type(type->front(), subst);

      // TypeVar that wasn't resolved during inference. Per the Dyn-rule
      // (Dyn is ONLY for `any`), a TypeVar from an unbound formal is a
      // strict error — assert in debug. But TypeVars that originated
      // from infer's `make_type()` for unannotated case-lambda return
      // types etc. are tolerated with a Dyn fallback (matching the
      // existing release-build behavior).
      //
      // Distinguish by checking seed_owner_map: a formal-seed TypeVar
      // has an entry there. An infer-leftover does not.
      if (type == TypeVar)
      {
        auto it = seed_owner_map.find(type->location());
        if (it != seed_owner_map.end())
        {
          // Formal seed — should have been bound by stage 2. If we
          // reach here, that's a real bug.
          assert(false && "reify_type(TypeVar): unbound formal reached IR boundary");
        }
        // Infer-leftover TypeVar — fall back to Dyn as in release.
        return Dyn;
      }

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
          // contains a union is flattened. TypeId entries for shapes with no
          // implementors are dropped (the code paths requiring them are
          // unreachable).
          if (rt == Dyn)
            return Dyn;
          else if (rt == Union)
            r << *rt;
          else if (rt == TypeId)
          {
            // Check if this TypeId corresponds to a shape with no
            // implementors. If the shape's reification hasn't been emitted
            // yet (null), keep it — resolve_shapes may add implementors
            // later. If the shape has been resolved to an empty union,
            // drop it. The post-shape cleanup pass (prune_empty_shape_unions)
            // handles any TypeIds that turn out to be empty after
            // resolve_shapes runs.
            auto has_empty_shape = [&](const Node& tid) -> bool {
              for (auto& key : map_order)
              {
                for (auto& cr : map[key])
                {
                  if (cr.id && same_reification_id(cr.id, tid))
                  {
                    if (cr.reification && (cr.reification == Type))
                    {
                      auto& body = cr.reification->back();
                      return (body == Union) && body->empty();
                    }

                    // Pending (null) or non-Type reification — keep.
                    return false;
                  }
                }
              }

              // Not in the map at all — treat as empty.
              return true;
            };

            if (!has_empty_shape(rt))
              r << rt;
          }
          else
            r << rt;
        }

        if (r->empty())
          return Dyn;

        if (r->size() == 1)
          return r->front();

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
          if (!same_reification_id(r.id, classid) || (r.def != ClassDef))
            continue;

          if (r.reification)
          {
            auto fields = r.reification / Fields;
            for (auto& field : *fields)
            {
              if ((field / FieldId)->location().view() != field_name)
                continue;
              return clone(field / Type);
            }
          }

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
    //
    // When `inner_ir_type` is a Union, ALSO reify ref[member] for each
    // member. This is needed because at runtime, `&x` for a var x of
    // declared type Union(T_1, ..., T_n) computes its dynamic type as
    // ref[actual_value_type], where actual_value_type is some T_i.
    // The runtime ref-type lookup (Program::ref) falls back to ref[Dyn]
    // if the precise ref[T_i] isn't reified, which then fails covariant
    // subtype checks against the expected ref[Union(...)].
    void ensure_ref_reified(const Node& inner_ir_type)
    {
      if (!inner_ir_type || (inner_ir_type == Dyn))
        return;

      // Recurse into union members so each ref[member] is also reified.
      if (inner_ir_type == Union)
      {
        for (auto& member : *inner_ir_type)
          ensure_ref_reified(member);
      }

      auto ref_defs = builtin->look(Location("ref"));
      assert(!ref_defs.empty());
      auto ref_def = ref_defs.front();
      auto tps = ref_def / TypeParams;
      assert(tps->size() == 1);

      Node expected_id = Ref << clone(inner_ir_type);
      NodeMap<Node> subst;
      subst[tps->at(0)] = clone(inner_ir_type);

      auto it = map.find(ref_def);
      bool is_new_key = (it == map.end());
      auto& r_vec = map[ref_def];

      if (is_new_key)
        map_order.push_back(ref_def);

      for (auto& existing : r_vec)
      {
        if (!existing.id->equals(expected_id))
          continue;

        if (existing.subst.empty())
        {
          existing.subst = subst;

          if (existing.reification)
          {
            for (auto& mi : method_invocations)
            {
              if (mi_targets(mi, existing.id))
                register_method(mi, existing);
            }
          }
        }

        return;
      }

      r_vec.push_back(
        {ref_def, std::move(subst), std::move(expected_id), {}, {}});
      register_with_worker(&r_vec.back());
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
          (((def / Lhs) == hand) || ((def / Lhs) == Once && hand == Rhs));
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
      auto classid =
        reify_emitted_type(type_node, subst, type_node, "constructed type");

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
      ReceiverSet receivers{true, {}};
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
         receivers.all,
         std::move(receivers.types)});

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

      if ((r.def / Shape) == Shape)
        return;

      // Skip method registration if the class has TypeParams but the
      // subst doesn't include them (e.g., wrapper classes created by
      // ensure_ref_reified with empty subst). Methods will be registered
      // when the subst is updated via find_or_push.
      auto class_tps = r.def / TypeParams;

      if (!class_tps->empty() && r.subst.empty())
        return;

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

    Nodes resolve_callback_targets(Node type)
    {
      if (!type)
        return {};

      if (type == Type)
        type = type->front();

      if (type == ClassId)
        return {clone(type)};

      if (type == Union)
      {
        Nodes targets;

        for (auto& child : *type)
        {
          auto resolved = resolve_callback_targets(child);
          targets.insert(targets.end(), resolved.begin(), resolved.end());
        }

        return targets;
      }

      if (type != TypeId)
        return {};

      auto type_id_loc = type->location().view();

      for (auto& key : map_order)
      {
        for (auto& r : map[key])
        {
          if (!r.id || (r.id->location().view() != type_id_loc))
            continue;

          if (!r.reification && (r.def == TypeAlias))
            reify_typealias(r);

          if (r.reification && (r.reification == Type))
            return resolve_callback_targets(r.reification->back());

          return {};
        }
      }

      return {};
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

    void emit_make_callback_error(
      Node& n, size_t match_count, bool has_generic) const
    {
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
          n, err(n, "make_callback requires exactly one 'apply' overload"));
      }
    }

    bool register_callback_type(
      const Node& type, Node site = {}, bool required = false)
    {
      auto targets = resolve_callback_targets(type);

      if (targets.empty())
      {
        auto inner = type;

        if (inner && (inner == Type))
          inner = inner->front();

        if (inner && (inner == TypeId))
        {
          pending_callbacks.push_back({site, clone(type), required});
          return true;
        }

        if (required && site)
        {
          site->parent()->replace(
            site, err(site, "make_callback: cannot determine lambda type"));
        }

        return false;
      }

      size_t match_count = 0;
      bool has_generic = false;

      for (auto& class_id : targets)
      {
        if (ensure_callback_method(class_id, &match_count, &has_generic))
          continue;

        if (required && site)
          emit_make_callback_error(site, match_count, has_generic);

        return false;
      }

      return true;
    }

    void process_pending_callbacks(bool final_pass)
    {
      std::vector<PendingCallback> remaining;

      for (auto& pending : pending_callbacks)
      {
        auto targets = resolve_callback_targets(pending.type);

        if (targets.empty())
        {
          if (final_pass && pending.required && pending.site)
          {
            pending.site->parent()->replace(
              pending.site,
              err(pending.site, "make_callback: cannot determine lambda type"));
          }
          else
          {
            remaining.push_back(
              {pending.site, clone(pending.type), pending.required});
          }

          continue;
        }

        size_t match_count = 0;
        bool has_generic = false;
        bool ok = true;

        for (auto& class_id : targets)
        {
          if (!ensure_callback_method(class_id, &match_count, &has_generic))
          {
            ok = false;
            break;
          }
        }

        if (ok)
          continue;

        if (final_pass && pending.required && pending.site)
          emit_make_callback_error(pending.site, match_count, has_generic);
      }

      pending_callbacks = std::move(remaining);
    }

    void reify_make_callback(Node& n, const Node& type)
    {
      register_callback_type(type, n, true);
    }

    // Reify init functions from a source Lib onto a reified Lib.
    // Checks for duplicate init across multiple Lib definitions
    // for the same library (by string name).
    void
    reify_initfini(const Node& source_lib, Node& reified_lib, Reification& r)
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
          auto prev = init_sources.at(reified_lib);

          errors.push_back(
            err(child / Ident, msg)
            << errmsg("Previous declaration resolved here:")
            << errloc(prev / Ident));
          continue;
        }

        // Reify the init function.
        auto funcid = find_or_push(child, r.subst);
        reified_lib->replace(existing, clone(funcid));
        init_sources[reified_lib] = child;
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
                reified_lib = Lib << clone(child / String) << Symbols << None;
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

                if ((lib_child / String)->location().view() != lib_loc.view())
                  continue;

                reify_initfini(lib_child, reified_lib, r);
              }

              // Reify the types in the symbol.
              Node ffi_params = FFIParams;

              for (auto& p : *(sym / FFIParams))
                ffi_params << reify_emitted_type(
                  p, r.subst, sym / SymbolId, "FFI parameter type");

              auto ret_type = reify_emitted_type(
                sym / Type, r.subst, sym / SymbolId, "FFI return type");

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
      auto when_type = n / Type;
      Node inner_type;

      if (when_type->front() != TypeVar)
      {
        inner_type = reify_type(when_type, r.subst);
      }
      else
      {
        // TypeVar return: try to resolve from the lambda apply's
        // registered method reification. The When's Rhs is the Lookup
        // result for 'apply' on the lambda.
        auto src_loc = (n / Rhs)->location();
        auto li = lookup_info.find(src_loc);

        if (li != lookup_info.end())
        {
          auto recv_it = local_types.find(li->second.recv_loc);

          if (recv_it != local_types.end())
          {
            auto ret =
              find_method_return_type(recv_it->second, li->second.method_id);

            if (ret)
              inner_type = ret;
          }
        }

        // Intermediate marker: WhenDyn cown content is unresolved at this
        // first-pass time (lambda's apply isn't yet reified). Use TypeVar
        // (not Dyn) so the second-pass refinement at line ~588 can detect
        // and replace it. Dyn is reserved for the IR encoding of `any`
        // per AGENTS.md. A post-pass safety net at line ~810 catches any
        // remaining TypeVar in WhenDyn cowns and emits an error.
        if (!inner_type)
          inner_type = make_typevar();
      }

      auto dst_loc = (n / LocalId)->location();
      auto result = WhenDyn << (n / LocalId) << (n / Rhs) << (n / Args)
                            << (Cown << inner_type);

      // Track the When result as Cown << inner_type.
      local_types[dst_loc] = Cown << clone(inner_type);
      return result;
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

          return err(
                   elem,
                   "Identifier not found: " +
                     std::string(ident->location().view()))
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
              {
                // Cycle detection: if we're already resolving this
                // TypeParam (i.e. its substitution transitively
                // references itself), break the cycle and emit a
                // proper compile error instead of looping until the
                // stack overflows. Self-referential substitutions
                // can arise from case-lambda type-param identity
                // confusion in infer, e.g. when a nested match
                // produces a substitution like
                // `T -> Union(TypeName(...::T), none)`.
                if (!resolving_typeparams.insert(d.get()).second)
                {
                  if (reported_unbound_formal.insert(d.get()).second)
                  {
                    errors.push_back(err(
                      elem,
                      std::format(
                        "Type parameter `{}` cannot be inferred "
                        "(self-referential substitution). Provide an "
                        "explicit type argument.",
                        (d / Ident)->location().view())));
                  }
                  return Dyn;
                }
                auto result = reify_type(find->second, resolve_subst);
                resolving_typeparams.erase(d.get());
                return result;
              }

              // TypeParam not in subst — hard compile error. Dyn is
              // ONLY the IR encoding of `any`; never a fallback.
              if (reported_unbound_formal.insert(d.get()).second)
              {
                errors.push_back(err(
                  elem,
                  std::format(
                    "Type parameter `{}` cannot be inferred. Provide an "
                    "explicit type argument.",
                    (d / Ident)->location().view())));
              }
              return Dyn;
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
          // When multiple defs exist (e.g., function overloads), pick
          // the one that contains the next element in the path.
          if (defs.size() > 1)
          {
            auto next_ident = (*(it + 1)) / Ident;
            def = {};

            for (auto& d : defs)
            {
              auto next_defs = d->look(next_ident->location());

              if (!next_defs.empty())
              {
                def = d;
                break;
              }
            }

            if (!def)
              def = defs.front();
          }
          else
          {
            def = defs.front();
          }

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

            // Bake `sub` so any implicit intermediate TypeArgs (paths
            // through generic outer scopes whose TypeParams come from
            // resolve_subst) are filled in. Without this, the inline
            // walk below would skip them via the `if (!sta->empty())`
            // guard, leaving the result context-dependent. Stored
            // values reaching here should already be self-contained
            // (resolve_typearg bakes via bake_typename before storage),
            // but baking again is a no-op fast-path, and defends
            // against any future code paths that bypass that storage
            // discipline.
            sub = bake_typename(sub, resolve_subst);

            // Navigate from the substituted TypeName to find the ClassDef.
            def = top;

            for (auto se_it = sub->begin(); se_it != sub->end(); ++se_it)
            {
              auto& se = *se_it;
              auto si = se / Ident;
              auto sta = se / TypeArgs;
              auto sdefs = def->look(si->location());

              if (sdefs.empty())
                return err(
                  se, "Definition not found in TypeParam resolution");

              // Disambiguate when multiple defs share an ident (e.g.
              // function overloads): pick the one whose body contains
              // the next element in the path, mirroring the outer
              // navigation loop's logic.
              bool is_se_last = (se_it + 1 == sub->end());

              if (!is_se_last && sdefs.size() > 1)
              {
                auto next_si = (*(se_it + 1)) / Ident;
                def = {};

                for (auto& d : sdefs)
                {
                  if (!d->look(next_si->location()).empty())
                  {
                    def = d;
                    break;
                  }
                }

                if (!def)
                  def = sdefs.front();
              }
              else
              {
                def = sdefs.front();
              }

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
            auto arg = ta->at(i);

            // A TypeArg that is `Type(TypeVar)` is a deferred placeholder
            // (Phase 3.5 partial infer_typeargs left this slot unbound
            // from arg evidence). Treat it as logically empty: try to
            // fill from `resolve_subst` (the ambient context) — this
            // handles cases like a call to `hmap::_lookup(...)` from
            // inside hmap's own method body, where infer leaves the
            // outer hmap.K / hmap.V TypeArgs as TypeVar but the call
            // site has them in scope.
            if (arg == Type && arg->front() == TypeVar)
            {
              auto find = resolve_subst.find(tps->at(i));
              if (find != resolve_subst.end())
              {
                r.subst[tps->at(i)] = find->second;
                resolve_subst[tps->at(i)] = find->second;
              }
              // else: leave unbound (deferred); will surface as a
              // hard error at the use site if not bound by Phase 3.5.
              continue;
            }

            // Substitute any TypeParam references in the TypeArg using the
            // full resolution context, to avoid self-referential cycles.
            auto resolved = resolve_typearg(arg, resolve_subst);
            r.subst[tps->at(i)] = resolved;
            resolve_subst[tps->at(i)] = resolved;
          }
        }
        else if (!tps->empty())
        {
          // No TypeArgs but the def has TypeParams (e.g., bare class
          // name as return type from within a generic class). Inherit
          // bindings from the ambient resolution context. If a
          // TypeParam isn't in resolve_subst, that's a hard compile
          // error — Dyn is ONLY the IR encoding of `any`, never a
          // fallback for unbound formals. (The previous existing-
          // reifications fallback was a workaround that gave the
          // WRONG answer when more than one reification of the
          // parent class existed: it picked the first one's bindings
          // regardless of context.)
          for (auto& tp : *tps)
          {
            auto find = resolve_subst.find(tp);

            if (find != resolve_subst.end())
            {
              r.subst[tp] = find->second;
              resolve_subst[tp] = find->second;
            }
            else
            {
              // Missing-from-subst: leave unbound. Either an explicit
              // Type(TypeVar) is reached via the deferred-binding
              // path (Phase 3.5), or a downstream consumer surfaces
              // the unbound-formal error at the actual use site.
            }
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

        resolved_name << (NameElement << clone(elem / Ident) << new_ta);
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

          if (
            (tp_find == subst.end()) ||
            has_unresolved_type(tp_find->second, subst))
            emit_unresolved_type_error(def / Ident, "wrapper element type");

          return wrap_find->second << elem_type;
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

  inline bool Reifier::ReifyWork::process(
    const Node& id, NodeWorker<Reifier::ReifyWork>& worker)
  {
    auto& s = worker.state(id);
    // s.reif may be null if this Node id was added via NodeWorker
    // dependency resolution but not via register_with_worker (the
    // sole path that sets reif). For now, treat as "nothing to do"
    // and mark Resolved so dependents unblock.
    if (s.reif == nullptr)
      return true;

    if (s.stage == State::Stage::Init)
    {
      // Stage 1: run the full per-Reification pipeline (signature +
      // body walk + emit + solve + IR build). Side-effect: every
      // find_or_push call records its returned id into
      // direct_deps_by_parent[id] via record_dep.
      outer->process_reification(*s.reif, id);
      s.stage = State::Stage::NeedSolve;

      // Block on all direct callees so that, when this reification
      // resumes for stage 2, every callee has had the chance to
      // emit its own body constraints into the global typevar_store.
      auto dep_it = outer->direct_deps_by_parent.find(id);
      if (dep_it != outer->direct_deps_by_parent.end() &&
          !dep_it->second.empty() &&
          worker.block_on_all(id, dep_it->second))
        return false;
      // Fall through if nothing to wait on.
    }

    if (s.stage == State::Stage::NeedSolve)
    {
      // Stage 2: re-query the constraint solver for formal_typevars
      // (now populated by callees' stage-1 emissions). If a formal
      // newly solves, update r.subst and propagate the binding into
      // the reified IR (Type, Params, Vars, Body Args).
      outer->reify_function_stage2(*s.reif);
      s.stage = State::Stage::Done;
    }

    return true;
  }

  inline void Reifier::reify_function_stage2(Reification& r)
  {
    if (!r.reification || r.reification != Func)
      return;

    // Collect (TypeParam, α_id) pairs to re-query: both formals
    // we seeded ourselves and any inherited TypeVar values in
    // r.subst (cross-function flow case).
    std::vector<std::pair<Node, uint32_t>> to_query;
    auto pf = reify_formal_alphas.find(&r);
    if (pf != reify_formal_alphas.end())
      to_query = pf->second;
    for (auto& [tp, val] : r.subst)
    {
      if (!val || val != Type || val->empty() ||
          val->front() != TypeVar)
        continue;
      auto seed_id = typevar_store.intern(val->front());
      bool dup = false;
      for (auto& [t, a] : to_query)
        if (a == seed_id)
        {
          dup = true;
          break;
        }
      if (!dup)
        to_query.emplace_back(tp, seed_id);
    }

    bool any_changed = false;
    for (auto& [tp, alpha_id] : to_query)
    {
      auto cur_subst_it = r.subst.find(tp);
      if (cur_subst_it == r.subst.end())
        continue;
      auto cur = cur_subst_it->second;
      if (!cur || cur != Type || cur->empty() ||
          cur->front() != TypeVar)
        continue;

      auto solved = typevar_store.solve(alpha_id);
      if (!solved || (solved == Union && solved->empty()))
        continue;
      bool has_tv = false;
      solved->traverse([&](const Node& n) {
        if (n == TypeVar)
          has_tv = true;
        return !has_tv;
      });
      if (has_tv)
        continue;

      // `solved` may itself be a Type-wrapped value or a bare type
      // expression. r.subst values are conventionally Type-wrapped,
      // so unwrap-then-wrap to avoid Type<Type<...>>.
      Node inner = (solved == Type && !solved->empty()) ?
        solved->front() : solved;
      r.subst[tp] = Type << clone(inner);
      any_changed = true;
    }

    if (!any_changed)
      return;

    // Re-emit the reified function signature using the updated
    // r.subst so consumers see the bound types.
    auto func = r.reification;

    // Return type.
    auto def_type = r.def / Type;
    if (def_type->front() != TypeVar)
    {
      auto new_ret = reify_emitted_type(
        def_type, r.subst, r.def / Ident, "return type");
      auto old_ret = func / Type;
      if (new_ret && !old_ret->equals(new_ret))
        func->replace(old_ret, new_ret);
    }

    // Param types.
    auto def_params = r.def / Params;
    auto func_params = func / Params;
    if (def_params->size() == func_params->size())
    {
      for (size_t i = 0; i < def_params->size(); i++)
      {
        auto new_pt = reify_type(def_params->at(i) / Type, r.subst);
        auto cur_pt = func_params->at(i) / Type;
        if (new_pt && !cur_pt->equals(new_pt))
          func_params->at(i)->replace(cur_pt, new_pt);
      }
    }

    // Var types (from TypeAssertions). Stage 1's first body walk
    // skipped pinning local_types for assertions whose type
    // referenced unbound formals (they had TypeVar seeds in subst).
    // After stage 2 binds those formals, walk r.def's body for any
    // TypeAssertion whose type is now fully resolved, and update
    // the matching VarDef's type to the user's declared type.
    Node vars = func / Vars;
    Node def_labels = r.def / Labels;
    if (vars && def_labels)
    {
      // Build location → asserted type map from r.def's body.
      std::map<Location, Node> asserted;
      def_labels->traverse([&](const Node& n) {
        if (n != TypeAssertion)
          return true;
        auto loc = (n / LocalId)->location();
        Node at = n / Type;
        if (!has_unresolved_type(at, r.subst))
        {
          auto reified = reify_type(at, r.subst);
          if (reified && reified != Dyn)
            asserted[loc] = reified;
        }
        return true;
      });

      for (auto& vd : *vars)
      {
        auto loc = (vd / LocalId)->location();
        auto it = asserted.find(loc);
        if (it == asserted.end())
          continue;
        Node cur = vd / Type;
        if (cur && !cur->equals(it->second))
          vd->replace(cur, clone(it->second));
      }
    }

    // Body Type fields: TypeTest's target type, NewArray element type,
    // etc. Stage 1 reified these with the still-seed subst, so they
    // ended up as Dyn (or worse). Re-walk the reified labels and
    // substitute Type-bearing fields using the updated subst.
    //
    // Each statement carries its Type as the LAST child (per wfIR).
    // We search by source location for matching def-statements and
    // re-reify their type.
    Node func_labels = func / Labels;
    if (def_labels && func_labels)
    {
      // Map source location → (statement type) from def's body for
      // statements that carry a Type field.
      std::map<Location, Node> stmt_type_at_loc;
      def_labels->traverse([&](const Node& n) {
        if (!n)
          return true;
        // Typetest in source has (LocalId * LocalId * Type) shape.
        if (n == Typetest)
        {
          Node nt = n / Type;
          if (nt && !has_unresolved_type(nt, r.subst))
          {
            auto reified = reify_type(nt, r.subst);
            if (reified)
              stmt_type_at_loc[n->location()] = reified;
          }
        }
        return true;
      });

      func_labels->traverse([&](const Node& n) {
        if (!n)
          return true;
        if (n == Typetest)
        {
          auto it = stmt_type_at_loc.find(n->location());
          if (it != stmt_type_at_loc.end())
          {
            Node cur = n->back();
            if (cur && !cur->equals(it->second))
              n->replace(cur, clone(it->second));
          }
        }
        return true;
      });
    }
  }

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
