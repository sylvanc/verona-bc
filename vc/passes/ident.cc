#include "../lang.h"

#include <trieste/nodeworker.h>

#include <map>
#include <set>
#include <string>

#define STEP(x) \
  switch (x) \
  { \
    case Done: \
      return Done; \
    case Delay: \
      return Delay; \
    case Continue: \
      break; \
  }

namespace vc
{
  struct Processor;

  struct Resolver
  {
    struct State : NodeWorkerState
    {
      Node result;
    };

    void seed(const Node&, State&) {}
    bool process(const Node& n, NodeWorker<Resolver>& worker);
  };

  struct Processor
  {
  private:
    enum StepResult
    {
      Done,
      Delay,
      Continue,
    };

    const Node& n;
    NodeWorker<Resolver>& worker;
    Resolver::State& state;
    Node top;
    Node found;

  public:
    Processor(const Node& n_, NodeWorker<Resolver>& worker_)
    : n(n_), worker(worker_), state(worker_.state(n_)), top(n_->parent({Top}))
    {
      assert(n->in({FuncName, TypeName}));
      assert(top == Top);
    }

    bool run()
    {
      if (steps() == Delay)
        return false;

      // This gets run on resolution, no matter which step returned Done.
      if (n->type() == state.result->type())
      {
        // Replace the contents. Don't replace `n`, as this is how NodeWorker
        // tracks progress.
        n->erase(n->begin(), n->end());
        n << *state.result;
        state.result = n;
      }
      else
      {
        // We've changed node type, replace `n`. This handles Error as well.
        n->parent()->replace(n, state.result);
      }

      return true;
    }

  private:
    StepResult steps()
    {
      STEP(block_on_children(n));

      // Don't try to resolve if any children had an error.
      if (n->get_contains_error())
        return Done;

      STEP(resolve_first());

      for (auto it = n->begin() + 1; it != n->end(); ++it)
      {
        STEP(resolve_alias());
        STEP(resolve_down(*it));
      }

      STEP(resolve_last());
      return Done;
    }

    // Build a fully qualified prefix from Top down to target_scope.
    // Each scope gets a NameElement with its Ident and empty TypeArgs.
    void build_fq_prefix(Node& result, const Node& target_scope)
    {
      for (auto& s : scope_path(target_scope))
      {
        auto scope_ident = s / Ident;
        result
          << (NameElement << (Ident ^ scope_ident->location()) << TypeArgs);
      }
    }

    StepResult resolve_first()
    {
      auto elem = n->front();
      auto name = elem / Ident;
      auto ta = elem / TypeArgs;
      auto curr_scope = n->parent({Top, ClassDef, TypeAlias, Function});
      state.result = (n == FuncName) ? FuncName : TypeName;

      while (curr_scope && !found)
      {
        // This gets from `name` up to `curr_scope`. That means `use` gets
        // repeated, which we have to catch below.
        auto defs = name->lookup(curr_scope);

        for (auto& def : defs)
        {
          // Check if this is a parameter or a local variable.
          if (def->in({ParamDef, Let, Var}) && (n == FuncName))
          {
            if (!def->precedes(n))
            {
              state.result = err(name, "Identifier used before definition");
            }
            else if (n->size() > 1)
            {
              state.result = err(
                name,
                "Identifier refers to a local but is used as a "
                "qualified name");
            }
            else if (!ta->empty())
            {
              // An identifier with type arguments is a method call of apply
              // with type arguments.
              state.result = Seq << (LocalId ^ name)
                                 << (Dot << (Ident ^ "apply") << ta);
            }
            else
            {
              // Local variable or parameter reference.
              state.result = LocalId ^ name;
            }

            return Done;
          }

          // Check if this is a class, type alias, type parameter, or function.
          // Functions are valid targets only for FuncName resolution.
          if (
            def->in({ClassDef, TypeAlias, TypeParam}) ||
            ((def == Function) && (n == FuncName)))
          {
            found = def;
            build_fq_prefix(state.result, curr_scope);
            state.result << elem;
            return Continue;
          }

          // Look into the imported names.
          if (
            (def == Use) &&
            (def->parent({Top, ClassDef, TypeAlias, Function}) == curr_scope))
          {
            // Don't follow our own Use (self-referential include).
            if (n->parent() == def)
              continue;

            // Ignore `use` that don't syntactically precede the definition.
            if (!def->precedes(name))
              continue;

            // Wait for the use to be resolved.
            STEP(block_on_children(def));

            // Ignore this if the use failed to resolve.
            auto use_name = def / TypeName;
            if (use_name == Error)
              continue;

            // The imported module will always be a ClassDef.
            auto use_def = find_def(use_name);
            assert(use_def == ClassDef);

            // This will be one ClassDef or TypeAlias, or zero or more
            // Functions. Use lookdown instead of look, so that we don't get
            // TypeParam or FieldDef results.
            auto defs = use_def->lookdown(name->location());
            if (defs.empty())
              continue;

            // The use_name is already fully qualified. Copy its elements
            // to state.result, preserving the correct type (FuncName/TypeName).
            state.result = (n == FuncName) ? FuncName : TypeName;

            for (auto& child : *use_name)
              state.result << clone(child);

            state.result << elem;
            found = defs.front();
            return Continue;
          }
        }

        curr_scope = curr_scope->parent({Top, ClassDef, TypeAlias, Function});
      }

      if ((n->size() == 1) && (n == FuncName))
      {
        // If it isn't found, treat it as a method if it's 1-element.
        state.result = MethodName << name << ta;
      }
      else
      {
        // Otherwise, it's an error.
        state.result = err(name, "Identifier not found");
      }

      return Done;
    }

    StepResult resolve_down(const Node& elem)
    {
      auto name = elem / Ident;

      // TODO: allow looking down a type parameter?
      if (found != ClassDef)
      {
        state.result = err(name, "Namespace elements must be a class")
          << errmsg("Resolving here:") << errloc(found / Ident);
        return Done;
      }

      // This will be one ClassDef or TypeAlias, or zero or more Functions.
      // Use lookdown instead of look, so that we don't get TypeParam or
      // FieldDef results.
      auto defs = found->lookdown(name->location());

      if (defs.empty())
      {
        state.result = err(name, "Identifier not found")
          << errmsg("Resolving here:") << errloc(found / Ident);
        return Done;
      }

      // If we have multiple functions, it doesn't matter which one we use.
      found = defs.front();
      state.result << elem;
      return Continue;
    }

    StepResult resolve_last()
    {
      auto name = n->back() / Ident;

      if (n->parent() == Use)
      {
        STEP(resolve_alias());

        if (found != ClassDef)
        {
          state.result = err(name, "Imports must be classes")
            << errmsg("Resolving here:") << errloc(found / Ident);
        }
      }
      else if (n == TypeName)
      {
        if (found == Function)
        {
          state.result = err(name, "Can't use a function as a type")
            << errmsg("Resolving here:") << errloc(found / Ident);
        }
      }
      else if (n == FuncName)
      {
        STEP(resolve_alias());

        if (found == ClassDef)
        {
          if ((found / Shape) == Shape)
          {
            state.result = err(name, "Can't instantiate a shape")
              << errmsg("Resolving here:") << errloc(found / Ident);
            return Done;
          }

          // Create sugar.
          auto defs = found->lookdown(Location("create"));

          if (defs.empty())
          {
            state.result = err(name, "Class has no 'create'")
              << errmsg("Resolving here:") << errloc(found / Ident);
            return Done;
          }

          // If there are multiple create functions, it doesn't matter which
          // one we use.
          found = defs.front();

          if (found != Function)
          {
            state.result = err(name, "Class 'create' must be a function")
              << errmsg("Resolving here:") << errloc(found / Ident);
            return Done;
          }

          state.result << (NameElement << (Ident ^ "create") << TypeArgs);
        }

        if (found == TypeParam)
        {
          // Create sugar.
          state.result << (NameElement << (Ident ^ "create") << TypeArgs);
          return Continue;
        }

        if (found != Function)
        {
          state.result = err(name, "Not a function")
            << errmsg("Resolving here:") << errloc(found / Ident);
        }
      }

      return Continue;
    }

    StepResult resolve_alias()
    {
      while (found == TypeAlias)
      {
        // Wait for the alias to be resolved.
        STEP(block_on_children(found));
        auto def = (found / Type)->front();

        if (def != TypeName)
        {
          state.result = err(def, "Type alias must be a type name")
            << errmsg("Resolving here:") << errloc(found / Ident);
          return Done;
        }

        state.result = substitute(state.result, def);
        found = find_def(state.result);
      }

      return Continue;
    }

    StepResult block_on_children(const Node& t)
    {
      Nodes blockers;

      t->traverse([&](Node& child) {
        if ((child != n) && child->in({FuncName, TypeName}))
          blockers.push_back(child);

        return true;
      });

      if (worker.block_on_all(n, blockers))
        return Delay;

      return Continue;
    }

    // Navigate a fully qualified name from Top to find the definition.
    Node find_def(const Node& name)
    {
      if (!name->in({FuncName, TypeName}))
        return {};

      Node def = top;

      for (auto& elem : *name)
      {
        assert(def != TypeParam);
        assert(elem == NameElement);
        auto defs = def->look((elem / Ident)->location());
        assert(!defs.empty());
        def = defs.front();
      }

      return def;
    }

    // Substitute TypeParams in `name` (the alias body, fully qualified) using
    // type arguments from `prefix` (the path that led to the alias).
    Node substitute(const Node& prefix, const Node& name)
    {
      assert(prefix->in({FuncName, TypeName}));
      assert(name->in({FuncName, TypeName}));

      // Recursively handle nested TypeName/FuncName refs in type args.
      auto rhs = clone(name);

      rhs->traverse([&](Node& node) {
        if ((node != rhs) && node->in({FuncName, TypeName}))
          node->parent()->replace(node, substitute(prefix, node));
        return true;
      });

      // Build a map from definition node to the prefix NameElement that
      // references it. This lets us look up type args for substitution.
      NodeMap<Node> scope_to_prefix_elem;
      auto def = top;

      for (auto& elem : *prefix)
      {
        assert(elem == NameElement);
        auto defs = def->look((elem / Ident)->location());
        assert(!defs.empty());
        def = defs.front();
        scope_to_prefix_elem[def] = elem;
      }

      // Walk through rhs, substituting TypeParams and carrying type args
      // from the prefix where the scopes match.
      Node result = prefix->type();
      auto curr = top;

      for (auto& elem : *rhs)
      {
        if (!result->in({FuncName, TypeName}))
        {
          return err(elem, "Can't look down through this type")
            << errmsg("Resolving here:") << errloc(result);
        }

        assert(elem == NameElement);
        auto defs = curr->look((elem / Ident)->location());
        assert(!defs.empty());
        def = defs.front();

        if (def == TypeParam)
        {
          // Find the TypeParam's index in the owning scope's TypeParams.
          auto tps = curr / TypeParams;
          size_t index = 0;

          for (auto& tp : *tps)
          {
            if (tp == def)
              break;

            index++;
          }

          // Look up the owning scope in the prefix to get type args.
          auto it = scope_to_prefix_elem.find(curr);

          if (it != scope_to_prefix_elem.end())
          {
            auto tas = it->second / TypeArgs;
            assert(index < tas->size());
            auto ta = tas->at(index);
            assert(ta == Type);
            result = clone(ta->front());

            // After substitution, navigate from where the result leads.
            if (result->in({FuncName, TypeName}))
              curr = find_def(result);
          }
          else
          {
            // No type args in prefix for this scope — keep as-is.
            result << clone(elem);
          }
        }
        else
        {
          // Carry type args from prefix if this scope appears there.
          auto new_elem = clone(elem);
          auto it = scope_to_prefix_elem.find(def);

          if (it != scope_to_prefix_elem.end())
          {
            auto prefix_ta = it->second / TypeArgs;

            if (!prefix_ta->empty())
              new_elem->replace(new_elem / TypeArgs, clone(prefix_ta));
          }

          result << new_elem;
          curr = def;
        }
      }

      return result;
    }
  };

  bool Resolver::process(const Node& n, NodeWorker<Resolver>& worker)
  {
    return Processor(n, worker).run();
  }

  // Alpha-rename shadowed local variables within a function so that each
  // variable binding gets a unique name. After the ident pass, references
  // are LocalId nodes and definitions are Let/Var nodes with Ident children.
  // If multiple Let/Var/ParamDef bindings share the same name within a
  // function, later bindings and their references are renamed to unique names.
  void alpha_rename(Node func)
  {
    // Collect all variable definitions (Let, Var, ParamDef) within this
    // function, in bottom-up order. Stop at nested Lambda boundaries.
    struct DefInfo
    {
      Node def;
      Node scope;
      std::string name;
    };

    Nodes scopes_bottomup;
    func->traverse([&](Node& node) {
      // Stop at nested Lambdas, but not if the node is func itself
      // (func may be a Lambda being processed).
      if ((node != func) && (node == Lambda))
        return false;
      if (node->in({Block, Function, Lambda}))
        scopes_bottomup.push_back(node);
      return true;
    });

    // Reverse so innermost scopes come first.
    std::reverse(scopes_bottomup.begin(), scopes_bottomup.end());

    // Group definitions by name across all scopes in this function.
    std::map<std::string, std::vector<DefInfo>> defs_by_name;

    for (auto& scope : scopes_bottomup)
    {
      for (auto& child : *scope)
      {
        if (child == Params)
        {
          for (auto& param : *child)
          {
            if (param == ParamDef)
            {
              auto name = std::string((param / Ident)->location().view());
              defs_by_name[name].push_back({param, scope, name});
            }
          }
        }
        else if (scope->in({Block, Function, Lambda}) && (child == Body))
        {
          child->traverse([&](Node& node) {
            // Don't descend into nested Blocks or Lambdas — their defs
            // are collected when we process their own scope entry.
            if (node->in({Lambda, Block}))
              return false;
            if (node->in({Let, Var}))
            {
              auto name = std::string((node / Ident)->location().view());
              defs_by_name[name].push_back({node, scope, name});
              return false;
            }
            return true;
          });
        }
      }
    }

    // For names with multiple definitions, rename all but the outermost.
    // Process innermost first (they appear first in scopes_bottomup).
    size_t rename_counter = 0;

    for (auto& [name, defs] : defs_by_name)
    {
      if (defs.size() <= 1)
        continue;

      // The outermost definition (last collected since we went bottom-up)
      // keeps its original name. Rename the rest.
      for (size_t i = 0; i < defs.size() - 1; i++)
      {
        auto& info = defs[i];
        auto new_name = name + "$" + std::to_string(rename_counter++);

        // Rename the definition's Ident child.
        auto ident = info.def / Ident;
        info.def->replace(ident, Ident ^ new_name);

        // Rename all LocalId nodes in the definition's scope that match
        // the original name. Since we process bottom-up, inner scopes
        // have already been renamed and won't match.
        auto& scope = info.scope;
        scope->traverse([&](Node& node) {
          // Stop at nested Lambdas, but not the scope itself.
          if ((node != scope) && (node == Lambda))
            return false;
          if ((node == LocalId) &&
              (node->location().view() == name))
          {
            auto replacement = LocalId ^ new_name;
            node->parent()->replace(node, replacement);
          }
          return true;
        });
      }
    }
  }

  PassDef ident()
  {
    auto nw = std::make_shared<NodeWorker<Resolver>>(Resolver{});

    PassDef p{
      "ident",
      wfPassIdent,
      dir::bottomup | dir::once,
      {
        T(FuncName, TypeName)[FuncName] >> [=](Match& _) -> Node {
          nw->add(_(FuncName));
          return NoChange;
        },
      }};

    p.post([=](Node top) {
      nw->run();

      for (auto& [n, state] : nw->states())
      {
        if (state.kind != WorkerStatus::Resolved)
        {
          auto e =
            err(n, "Could not resolve identifier due to circular dependencies");

          for (auto& dep : state.dependents)
            e << errmsg("Dependent on this:") << errloc(dep);

          n->parent()->replace(n, e);
        }
      }

      // Collect Use nodes to remove, then remove them after traversal.
      // Removing during traversal would invalidate the traverse iterator.
      Nodes uses;
      top->traverse([&](Node& node) {
        if ((node == Use) && !node->get_contains_error())
          uses.push_back(node);
        return true;
      });

      for (auto& use : uses)
        use->parent()->replace(use);

      // Alpha-rename shadowed local variables so each binding within a
      // function has a unique name.
      top->traverse([](Node& node) {
        if (node->in({Function, Lambda}))
          alpha_rename(node);
        return true;
      });

      return 0;
    });

    return p;
  }
}
