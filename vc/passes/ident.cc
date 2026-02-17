#include "../lang.h"

#include <trieste/nodeworker.h>

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
    Node scope;
    Node found;

  public:
    Processor(const Node& n_, NodeWorker<Resolver>& worker_)
    : n(n_),
      worker(worker_),
      state(worker_.state(n_)),
      scope(n_->parent({Top, ClassDef, TypeAlias, Function}))
    {
      assert(n->in({FuncName, TypeName}));
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

    StepResult resolve_first()
    {
      auto elem = n->front();
      auto name = elem / Ident;
      auto ta = elem / TypeArgs;
      auto curr_scope = scope;
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

          // Check if this is a class, type alias, or type parameter.
          if (def->in({ClassDef, TypeAlias, TypeParam}))
          {
            found = def;
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
            auto use_def = find_def(use_name, curr_scope);
            assert(use_def == ClassDef);

            // This will be one ClassDef or TypeAlias, or zero or more
            // Functions. Use lookdown instead of look, so that we don't get
            // TypeParam or FieldDef results.
            auto defs = use_def->lookdown(name->location());
            if (defs.empty())
              continue;

            // Update state.result to include the use name.
            state.result = concat(state.result, use_name, scope);
            state.result << elem;
            found = defs.front();
            return Continue;
          }
        }

        state.result << TypeParent;
        curr_scope = curr_scope->parent({Top, ClassDef, TypeAlias, Function});
      }

      if ((n->size() == 1) && (n == FuncName))
      {
        // If it isn't found, treat it as a method if it's 1-element.
        state.result = MethodName << name << ta;
      }
      else if (
        (n->size() == 1) && (n == TypeName) && ta->empty() &&
        (name->location().view() == "Self"))
      {
        // Accept `Self` for now as a hack.
        state.result << elem;
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

        state.result = concat(state.result, def, scope);
        found = find_def(state.result, scope);
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

    Node find_def(const Node& name, const Node& from)
    {
      if (!name->in({FuncName, TypeName}))
        return {};

      Node def = from;

      for (auto elem : *name)
      {
        // TODO: what if def is a TypeParam?
        assert(def != TypeParam);

        if (elem == TypeParent)
        {
          def = def->parent({Top, ClassDef, TypeAlias, Function});
          assert(def);
        }
        else
        {
          auto defs = def->look((elem / Ident)->location());
          assert(!defs.empty());
          def = defs.front();
        }
      }

      return def;
    }

    Node concat(const Node& prefix, const Node& name, const Node& scope)
    {
      // Both sides are already resolved.
      assert(prefix->in({FuncName, TypeName}));
      assert(name->in({FuncName, TypeName}));

      // Concat the type arguments.
      auto rhs = clone(name);

      rhs->traverse([&](Node& node) {
        if ((node != rhs) && node->in({FuncName, TypeName}))
          node->parent()->replace(node, concat(prefix, node, scope));
        return true;
      });

      // Concat the name.
      auto curr_scope = find_def(prefix, scope);
      auto lhs = clone(prefix);

      for (auto& elem : *rhs)
      {
        if (!lhs->in({FuncName, TypeName}))
        {
          // This can only happen if we expanded a type parameter reference to
          // something that isn't a TypeName, and then we look down.
          return err(elem, "Can't look down through this type")
            << errmsg("Resolving here:") << errloc(lhs);
        }

        if (elem == TypeParent)
        {
          if (lhs->empty() || (lhs->back() == TypeParent))
            lhs << elem;
          else
            lhs->pop_back();

          curr_scope = curr_scope->parent({Top, ClassDef, TypeAlias, Function});
        }
        else
        {
          assert(curr_scope);
          auto defs = curr_scope->look((elem / Ident)->location());
          assert(!defs.empty());
          auto def = defs.front();

          if (def == TypeParam)
          {
            // If lhs ends with TypeParent, there are no type arguments
            // to substitute — the type parameter is from an enclosing
            // scope reached via parent navigation, not via an explicit
            // name with type arguments.
            if (lhs->empty() || lhs->back() == TypeParent)
            {
              lhs << elem;
            }
            else
            {
              auto tps = curr_scope / TypeParams;
              size_t index = 0;

              for (auto& tp : *tps)
              {
                if (tp == def)
                  break;

                index++;
              }

              auto tas = lhs->back() / TypeArgs;
              assert(index < tas->size());
              auto ta = tas->at(index);
              assert(ta == Type);
              lhs = clone(ta->front());
              curr_scope = scope;
            }
          }
          else
          {
            lhs << elem;
          }
        }
      }

      return lhs;
    }
  };

  bool Resolver::process(const Node& n, NodeWorker<Resolver>& worker)
  {
    return Processor(n, worker).run();
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

      return 0;
    });

    return p;
  }
}
