#include "../lang.h"

namespace vbcc
{
  PassDef optimize(std::shared_ptr<Bytecode> state)
  {
    PassDef p{
      "optimize",
      wfIR,
      dir::once,
      {
        // No rewrite rules — all work done in post().
      }};

    p.post([state](auto top) {
      // Helper: find a Class definition by ClassId.
      auto find_class = [&](const Node& class_id) -> Node {
        for (auto& child : *top)
        {
          if (
            child == Class &&
            (child / ClassId)->location() == class_id->location())
            return child;
        }
        return {};
      };

      // Helper: find a Primitive definition by type token.
      auto find_primitive = [&](const Node& type_node) -> Node {
        for (auto& child : *top)
        {
          if (child == Primitive && (child / Type)->type() == type_node->type())
            return child;
        }
        return {};
      };

      // Helper: check if a type token is a simple primitive.
      auto is_primitive = [](const Node& t) -> bool {
        return t->type().in(
          {None,
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
           Ptr,
           Callback});
      };

      // Helper: find a Func definition by FunctionId.
      auto find_func = [&](const Node& func_id) -> Node {
        for (auto& child : *top)
        {
          if (
            child->type().in({Func, FuncOnce}) &&
            (child / FunctionId)->location() == func_id->location())
            return child;
        }
        return {};
      };

      // Helper: resolve a receiver type + method_id to a FunctionId node.
      // Returns empty Node if unresolvable.
      auto resolve_method =
        [&](const Node& src_type, const Node& method_id) -> Node {
        if (!src_type)
          return {};

        // Cannot devirtualize unions or Dyn.
        if (src_type == Union || src_type == Dyn)
          return {};

        Node cls;
        if (src_type == ClassId)
          cls = find_class(src_type);
        else if (is_primitive(src_type))
          cls = find_primitive(src_type);
        else if (src_type->type().in({Array, Cown, Ref}))
        {
          for (auto& child : *top)
          {
            if (child == Primitive && (child / Type)->type() == src_type->type())
            {
              cls = child;
              break;
            }
          }
        }

        if (!cls)
          return {};

        for (auto& method : *(cls / Methods))
        {
          if ((method / MethodId)->location() == method_id->location())
            return method / FunctionId;
        }
        return {};
      };

      // Process each function.
      for (auto& func_node : *top)
      {
        if (!func_node->type().in({Func, FuncOnce}))
          continue;

        auto func_key =
          std::string((func_node / FunctionId)->location().view());
        auto lookups_it = state->func_lookups.find(func_key);

        if (lookups_it == state->func_lookups.end())
          continue;

        auto& lookups = lookups_it->second;

        // Collect dead lookup register names (Phase C will remove them).
        std::set<std::string> dead_lookups;

        // Phase A: Devirtualize.
        // Phase B: Inline trivial calls.
        // We do both in a single pass through each label's body, since
        // Phase A produces Call nodes that Phase B can immediately inline.
        for (auto& label : *(func_node / Labels))
        {
          auto body = label / Body;

          // Collect replacements (can't mutate during iteration).
          std::vector<std::pair<Node, Node>> replacements;

          for (auto& stmt : *body)
          {
            // Phase A: Devirtualize CallDyn.
            if (stmt == CallDyn || stmt == TryCallDyn)
            {
              auto fn_ptr_name =
                std::string((stmt / Rhs)->location().view());
              auto it = lookups.find(fn_ptr_name);

              if (it == lookups.end())
                continue;

              auto& info = it->second;
              auto func_id = resolve_method(info.src_type, info.method_id);

              if (stmt == CallDyn)
              {
                if (!func_id)
                  continue;

                // Replace CallDyn with Call.
                Node call = Call << clone(stmt / LocalId)
                                 << clone(func_id) << clone(stmt / Args);
                replacements.push_back({stmt, call});
                dead_lookups.insert(fn_ptr_name);
              }
              else // TryCallDyn
              {
                if (func_id)
                {
                  // Method exists — replace with Call.
                  Node call = Call << clone(stmt / LocalId)
                                   << clone(func_id) << clone(stmt / Args);
                  replacements.push_back({stmt, call});
                }
                else if (
                  info.src_type && info.src_type != Union &&
                  info.src_type != Dyn)
                {
                  // Known non-union type, method doesn't exist — produce
                  // none.
                  Node none_const =
                    Const << clone(stmt / LocalId) << None << None;
                  replacements.push_back({stmt, none_const});
                }
                else
                {
                  continue;
                }
                dead_lookups.insert(fn_ptr_name);
              }
            }
            // Phase A: Devirtualize WhenDyn.
            else if (stmt == WhenDyn)
            {
              auto fn_ptr_name =
                std::string((stmt / Rhs)->location().view());
              auto it = lookups.find(fn_ptr_name);

              if (it == lookups.end())
                continue;

              auto& info = it->second;
              auto func_id = resolve_method(info.src_type, info.method_id);

              if (!func_id)
                continue;

              // Replace WhenDyn with When.
              Node when = When << clone(stmt / LocalId) << clone(func_id)
                               << clone(stmt / Args) << clone(stmt / Cown);
              replacements.push_back({stmt, when});
              dead_lookups.insert(fn_ptr_name);
            }
          }

          // Apply Phase A replacements.
          for (auto& [old_node, new_node] : replacements)
            old_node->parent()->replace(old_node, new_node);

          // Phase B: Inline trivial calls.
          // Re-scan body since replacements may have changed it.
          std::vector<std::pair<Node, Node>> inline_replacements;

          for (auto& stmt : *body)
          {
            if (stmt != Call)
              continue;

            auto target = find_func(stmt / FunctionId);

            if (!target)
              continue;

            // Don't inline FuncOnce (has memoization semantics).
            if (target == FuncOnce)
              continue;

            auto labels_node = target / Labels;

            if (labels_node->size() != 1)
              continue;

            auto target_label = labels_node->front();
            auto target_body = target_label / Body;

            if (target_body->size() != 1)
              continue;

            auto target_term = target_label->back();

            if (target_term != Return)
              continue;

            auto target_stmt = target_body->front();

            // Don't inline statements that themselves make calls.
            if (target_stmt->type().in(
                  {Call,
                   CallDyn,
                   TryCallDyn,
                   FFI,
                   When,
                   WhenDyn,
                   MemoSlot}))
              continue;

            // Check that the return value is the statement's dst.
            auto target_ret_local = target_term / LocalId;
            auto target_dst = target_stmt / LocalId;

            if (target_ret_local->location() != target_dst->location())
              continue;

            // Build param→arg mapping.
            auto params = target / Params;
            auto args = stmt / Args;

            if (params->size() != args->size())
              continue;

            std::unordered_map<std::string, std::string> param_map;
            auto p_it = params->begin();
            auto a_it = args->begin();

            while (p_it != params->end() && a_it != args->end())
            {
              auto param_name =
                std::string(((*p_it) / LocalId)->location().view());
              auto arg_name =
                std::string(((*a_it) / Rhs)->location().view());
              param_map[param_name] = arg_name;
              ++p_it;
              ++a_it;
            }

            // Clone the target statement and remap locals.
            Node inlined = clone(target_stmt);
            auto call_dst_loc = (stmt / LocalId)->location();

            // Replace dst: swap the inlined statement's dst LocalId.
            auto inlined_dst = inlined / LocalId;
            inlined->replace(inlined_dst, LocalId ^ call_dst_loc);

            // Recursively remap all LocalId descendants that match
            // callee params to caller args. Skip the dst (already done).
            // Also change ArgMove→ArgCopy in Arg nodes for remapped
            // params, since the caller owns the register and we must
            // not move from it.
            auto dst_loc = (inlined / LocalId)->location();
            std::function<void(Node&)> remap_locals = [&](Node& n) {
              for (size_t i = 0; i < n->size(); i++)
              {
                auto child = n->at(i);

                if (
                  child == LocalId && child->location() != dst_loc)
                {
                  auto name = std::string(child->location().view());
                  auto map_it = param_map.find(name);

                  if (map_it != param_map.end())
                  {
                    n->replace(child, LocalId ^ map_it->second);

                    // If this LocalId is inside an Arg node, change
                    // ArgMove to ArgCopy since we're now directly
                    // referencing the caller's register.
                    if (n == Arg && (n / Type) == ArgMove)
                      n->replace(n / Type, ArgCopy);
                  }
                }
                else if (child->size() > 0)
                {
                  remap_locals(child);
                }
              }
            };
            remap_locals(inlined);

            inline_replacements.push_back({stmt, inlined});
          }

          // Apply Phase B replacements.
          for (auto& [old_node, new_node] : inline_replacements)
            old_node->parent()->replace(old_node, new_node);

          // Phase C: Remove dead Lookups and Drops.
          // Only remove a Lookup if no remaining statement uses it.
          std::set<std::string> still_used;

          for (auto& stmt : *body)
          {
            if (stmt->type().in({CallDyn, TryCallDyn, WhenDyn}))
            {
              auto name = std::string((stmt / Rhs)->location().view());
              still_used.insert(name);
            }
          }

          std::vector<Node> to_remove;

          for (auto& stmt : *body)
          {
            if (stmt == Lookup)
            {
              auto dst_name =
                std::string((stmt / LocalId)->location().view());

              if (
                dead_lookups.count(dst_name) &&
                !still_used.count(dst_name))
                to_remove.push_back(stmt);
            }
            else if (stmt == Drop)
            {
              auto drop_name =
                std::string((stmt / LocalId)->location().view());

              if (
                dead_lookups.count(drop_name) &&
                !still_used.count(drop_name))
                to_remove.push_back(stmt);
            }
          }

          for (auto& node : to_remove)
            node->parent()->replace(node);
        }

        // Phase A for terminators: Devirtualize TailcallDyn.
        for (auto& label : *(func_node / Labels))
        {
          auto term = label->back();

          if (term != TailcallDyn)
            continue;

          auto fn_ptr_name = std::string((term / LocalId)->location().view());
          auto it = lookups.find(fn_ptr_name);

          if (it == lookups.end())
            continue;

          auto& info = it->second;
          auto func_id = resolve_method(info.src_type, info.method_id);

          if (!func_id)
            continue;

          Node tailcall =
            Tailcall << clone(func_id) << clone(term / MoveArgs);
          label->replace(term, tailcall);
          dead_lookups.insert(fn_ptr_name);
        }
      }

      return 0;
    });

    return p;
  }
}
