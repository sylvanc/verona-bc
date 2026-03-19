#include "../lang.h"

namespace vbcc
{
  PassDef memo()
  {
    auto p = PassDef{"memo", wfIR, dir::once};

    p.post([](Node top) {
      // Collect FuncOnce nodes and build a function map.
      std::set<std::string> memo_ids;
      std::unordered_map<std::string, Node> func_map;

      for (auto& child : *top)
      {
        if (child == FuncOnce)
        {
          auto id_str = std::string((child / FunctionId)->location().view());
          memo_ids.insert(id_str);
          func_map[id_str] = child;
        }
        else if (child == Func)
        {
          auto id_str = std::string((child / FunctionId)->location().view());
          func_map[id_str] = child;
        }
      }

      if (memo_ids.empty())
        return 0;

      // --- Helper lambdas for CallDyn resolution ---

      auto find_func = [&](const std::string& id) -> Node {
        auto it = func_map.find(id);
        return (it != func_map.end()) ? it->second : Node{};
      };

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

      auto find_primitive = [&](const Node& type_node) -> Node {
        for (auto& child : *top)
        {
          if (child == Primitive && (child / Type)->type() == type_node->type())
            return child;
        }
        return {};
      };

      // Resolve a type to the class/primitive that has methods.
      auto find_class_or_prim = [&](const Node& type_node) -> Node {
        if (!type_node)
          return {};
        if (type_node == ClassId)
          return find_class(type_node);
        if (type_node->type().in(
              {None, Bool, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64,
               ILong, ULong, ISize, USize, Ptr, Callback}))
          return find_primitive(type_node);
        if (type_node->type().in({Array, Cown, Ref}))
        {
          for (auto& child : *top)
          {
            if (
              child == Primitive && (child / Type)->type() == type_node->type())
              return child;
          }
        }
        return {};
      };

      // Resolve a method on a type to a FunctionId string.
      auto resolve_method =
        [&](const Node& type_node, const Node& method_id) -> std::string {
        auto cls = find_class_or_prim(type_node);
        if (!cls)
          return {};
        for (auto& method : *(cls / Methods))
        {
          if ((method / MethodId)->location() == method_id->location())
            return std::string(
              (method / FunctionId)->location().view());
        }
        return {};
      };

      // Build a lightweight type environment for a function body.
      // Maps register name -> type node (just enough to resolve Lookup
      // source types).
      auto build_type_env =
        [&](Node func_node) -> std::unordered_map<std::string, Node> {
        std::unordered_map<std::string, Node> env;

        // Parameters.
        for (auto& param : *(func_node / Params))
        {
          auto name = std::string((param / LocalId)->location().view());
          env[name] = param / Type;
        }

        // Body statements.
        for (auto& label : *(func_node / Labels))
        {
          for (auto& stmt : *(label / Body))
          {
            if (stmt->in({New, Stack, Heap, Region}))
            {
              auto name = std::string((stmt / LocalId)->location().view());
              env[name] = stmt / ClassId;
            }
            else if (stmt == Const)
            {
              auto name = std::string((stmt / LocalId)->location().view());
              env[name] = stmt / Type;
            }
            else if (stmt->in({Copy, Move}))
            {
              auto name = std::string((stmt / LocalId)->location().view());
              auto src = std::string((stmt / Rhs)->location().view());
              auto it = env.find(src);
              if (it != env.end())
                env[name] = it->second;
            }
            else if (stmt == Call)
            {
              auto name = std::string((stmt / LocalId)->location().view());
              auto target = find_func(
                std::string((stmt / FunctionId)->location().view()));
              if (target)
                env[name] = target / Type;
            }
          }
        }

        return env;
      };

      // --- Dependency graph ---

      // edges[a] = {b, c, ...} means once-function a depends on
      // once-functions b, c, ...
      std::unordered_map<std::string, std::set<std::string>> edges;
      std::unordered_map<std::string, std::unordered_map<std::string, Node>>
        edge_sites;

      for (auto& id : memo_ids)
        edges[id] = {};

      // Build dependency graph with per-root visited sets to prevent
      // infinite recursion through non-once mutual recursion.
      std::unordered_map<std::string, std::set<std::string>> visited_per_root;

      auto find_deps_with_visited = [&](
                                      const std::string& root,
                                      Node func_node,
                                      auto& self) -> void {
        auto& visited = visited_per_root[root];
        auto func_id =
          std::string((func_node / FunctionId)->location().view());

        if (!visited.insert(func_id).second)
          return; // Already explored this function for this root.

        // Build type env for CallDyn resolution.
        auto env = build_type_env(func_node);

        // Function-scoped lookup info for cross-label CallDyn resolution.
        std::unordered_map<std::string, std::pair<Node, Node>> lookup_info;

        for (auto& label : *(func_node / Labels))
        {
          for (auto& stmt : *(label / Body))
          {
            if (stmt == Lookup)
            {
              auto dst_name =
                std::string((stmt / LocalId)->location().view());
              auto src_name = std::string((stmt / Rhs)->location().view());
              auto src_it = env.find(src_name);
              Node src_type = src_it != env.end() ? src_it->second : Node{};
              lookup_info[dst_name] = {src_type, stmt / MethodId};
            }
            else if (stmt == Call)
            {
              auto target =
                std::string((stmt / FunctionId)->location().view());
              if (memo_ids.count(target))
              {
                edges[root].insert(target);
                edge_sites[root].try_emplace(target, stmt);
              }
              else
              {
                auto f = find_func(target);
                if (f)
                  self(root, f, self);
              }
            }
            else if (stmt->in({CallDyn, TryCallDyn}))
            {
              auto fn_ptr_name =
                std::string((stmt / Rhs)->location().view());
              auto it = lookup_info.find(fn_ptr_name);
              if (it == lookup_info.end())
                continue;

              auto& [src_type, method_id] = it->second;
              if (!src_type)
                continue;

              auto resolve_for_type = [&](const Node& t) {
                auto func_id_str = resolve_method(t, method_id);
                if (func_id_str.empty())
                  return;
                if (memo_ids.count(func_id_str))
                {
                  edges[root].insert(func_id_str);
                  edge_sites[root].try_emplace(func_id_str, stmt);
                }
                else
                {
                  auto f = find_func(func_id_str);
                  if (f)
                    self(root, f, self);
                }
              };

              if (src_type == Union)
              {
                for (auto& member : *src_type)
                  resolve_for_type(member);
              }
              else
              {
                resolve_for_type(src_type);
              }
            }
          }
        }
      };

      for (auto& id : memo_ids)
      {
        auto func = func_map.at(id);
        find_deps_with_visited(id, func, find_deps_with_visited);
      }

      // --- Topological sort with cycle detection ---

      enum class Mark
      {
        None,
        Temp,
        Perm
      };
      std::unordered_map<std::string, Mark> marks;
      std::vector<std::string> sorted;
      bool has_cycle = false;
      std::vector<std::string> cycle_path;

      auto topo_visit = [&](const std::string& id, auto& self) -> void {
        if (has_cycle)
          return;

        auto& mark = marks[id];
        if (mark == Mark::Perm)
          return;
        if (mark == Mark::Temp)
        {
          has_cycle = true;
          cycle_path.push_back(id);
          return;
        }

        mark = Mark::Temp;
        for (auto& dep : edges[id])
        {
          self(dep, self);
          if (has_cycle)
          {
            cycle_path.push_back(id);
            return;
          }
        }
        mark = Mark::Perm;
        sorted.push_back(id);
      };

      for (auto& id : memo_ids)
      {
        topo_visit(id, topo_visit);
        if (has_cycle)
          break;
      }

      if (has_cycle)
      {
        // Build cycle description.
        std::string msg = "once functions form a cycle: ";
        for (size_t i = 0; i < cycle_path.size(); i++)
        {
          if (i > 0)
            msg += " -> ";
          msg += cycle_path[i];
        }

        Node site;
        if (cycle_path.size() > 1)
        {
          auto root = cycle_path.front();
          auto target = cycle_path[1];
          auto root_it = edge_sites.find(root);
          if (root_it != edge_sites.end())
          {
            auto site_it = root_it->second.find(target);
            if (site_it != root_it->second.end())
              site = site_it->second;
          }
        }

        if (site)
          top << err(site, msg);
        else
          top << err(func_map.at(cycle_path.back()) / FunctionId, msg);
        return 1;
      }

      assert(sorted.size() == memo_ids.size());

      // --- Split each FuncOnce into stub + init ---

      // Collect FuncOnce nodes to replace (can't mutate during iteration).
      std::vector<Node> to_replace;
      for (auto& child : *top)
      {
        if (child == FuncOnce)
          to_replace.push_back(child);
      }

      for (auto& func_once : to_replace)
      {
        auto orig_id = func_once / FunctionId;
        auto orig_id_str =
          std::string(orig_id->location().view());
        auto init_id_str = orig_id_str + "$once";
        auto r_type = func_once / Type;
        auto params = func_once / Params;
        auto vars = func_once / Vars;
        auto labels = func_once / Labels;

        // Init function: new id, original body.
        Node init_func = Func << (FunctionId ^ init_id_str) << params
                              << clone(r_type) << vars << labels;

        // Stub function: original id, MemoSlot load.
        Node stub_func =
          Func << (FunctionId ^ orig_id_str) << Params << clone(r_type)
               << (Vars << (LocalId ^ "$memo"))
               << (Labels
                   << (Label << (LabelId ^ "entry")
                             << (Body
                                 << (MemoSlot << (LocalId ^ "$memo")
                                              << (FunctionId ^ init_id_str)))
                             << (Return << (LocalId ^ "$memo"))));

        // Replace FuncOnce with both Func nodes.
        auto it = top->find(func_once);
        it = top->insert(it, stub_func);
        it = top->insert(std::next(it), init_func);
        auto erase_it = top->find(func_once);
        top->erase(erase_it, std::next(erase_it));

        // Update func_map for the new functions.
        func_map[orig_id_str] = stub_func;
        func_map[init_id_str] = init_func;
      }

      // --- Emit MemoInit ---

      Node memo_init = MemoInit;
      for (auto& id : sorted)
      {
        auto init_id_str = id + "$once";
        memo_init << (FunctionId ^ init_id_str);
      }
      top << memo_init;

      return 0;
    });

    return p;
  }
}
