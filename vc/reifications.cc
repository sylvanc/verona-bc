#include "reifications.h"

#include "subtype.h"

namespace vc
{
  void Reifications::start(Node t)
  {
    top = t;

    // Get builtin.
    builtin = top->lookdown(Location("builtin")).front();

    // Assume the main module is the first one.
    auto main_module = top->front();
    assert(main_module == ClassDef);
    assert((main_module / TypeParams)->empty());

    // Look for an LHS function with no parameters.
    auto defs = main_module->lookdown(Location("main"));
    Node main;

    for (auto& def : defs)
    {
      if (
        (def != Function) || ((def / Lhs) != Rhs) || (!(def / Params)->empty()))
        continue;

      main = def;
      break;
    }

    if (!main)
    {
      top->replace(main_module, err(main_module, "No main function."));
      return;
    }

    if (!(main / TypeParams)->empty())
    {
      main_module->replace(
        main, err(main, "Main function can't have type parameters."));
      return;
    }

    auto&& [r, fresh] = schedule(main, {}, true);

    // Add an entry point for main.
    auto id = top->fresh();
    top
      << (Func << (FunctionId ^ "@main") << Params << I32 << Vars
               << (Labels
                   << (Label << (LabelId ^ "start")
                             << (Body
                                 << (Call << (LocalId ^ id)
                                          << clone(r.instance / Ident) << Args))
                             << (Return << (LocalId ^ id)))));
  }

  void Reifications::run()
  {
    while (!wl.empty())
    {
      auto [wl_def, wl_i] = wl.top();

      if (wl_def == Lookup)
      {
        // Lookups don't get delayed, so pop it immediately.
        wl.pop();

        // For each completed, reified class, check for a compatible function,
        // reify the function, and add the method.
        auto hand = (wl_def / Lhs)->type();
        auto name = (wl_def / Ident)->location();
        auto ta = wl_def / TypeArgs;
        auto ta_count = ta->size();
        auto arg_count = parse_int(wl_def / Int);
        auto method_id = wl_def->back();

        std::for_each(map.begin(), map.end(), [&](auto& kv) {
          if (kv.first != ClassDef)
            return;

          auto defs = kv.first->lookdown(name);
          Node func;

          for (auto& def : defs)
          {
            if (
              (def == Function) && ((def / Lhs) == hand) &&
              ((def / TypeParams)->size() == ta_count) &&
              ((def / Params)->size() == arg_count))
            {
              // We have a compatible function.
              func = def;
              break;
            }
          }

          if (!func)
            return;

          auto tp = func / TypeParams;

          for (auto& r : kv.second)
          {
            if ((r.status != Ok) || !r.instance)
              continue;

            // Extend the class substitution map with the new type arguments.
            auto subst = r.subst_orig;

            for (size_t i = 0; i < tp->size(); i++)
              subst[tp->at(i)] = ta->at(i);

            // TODO: if this fails, don't report an error, and delete the
            // reification.
            auto&& [r_func, fresh] = schedule(func, subst, true);
            r_func.want_method(r.instance, method_id);
          }
        });

        continue;
      }

      auto& r = map[wl_def][wl_i];

      if (r.status != Delay)
      {
        // Don't pop at the end, since we may have scheduled more work on the
        // stack that needs to be processed.
        wl.pop();
        continue;
      }

      r.run();
    }
  }

  std::pair<Reification&, bool>
  Reifications::schedule(Node def, Subst subst, bool enqueue)
  {
    // Remove unassociated type parameters.
    if (def->in({ClassDef, TypeAlias, Function}))
    {
      Nodes remove;

      std::for_each(subst.begin(), subst.end(), [&](auto& kv) {
        auto& tp = kv.first;
        auto name = (tp / Ident)->location();
        auto found = false;
        auto p = def;

        while (p)
        {
          auto tps = p / TypeParams;

          if (std::any_of(
                tps->begin(), tps->end(), [&](auto& d) { return d == tp; }))
          {
            found = true;
            break;
          }

          p = p->parent(ClassDef);
        }

        if (!found)
          remove.push_back(tp);
      });

      for (auto& tp : remove)
        subst.erase(tp);
    }

    // Search for an existing reification with invariant substitutions.
    auto& r = map[def];
    size_t i = 0;
    bool fresh = false;

    for (; i < r.size(); i++)
    {
      // If we have an existing reification with invariant substitutions,
      // return that.
      if (std::equal(
            r[i].subst.begin(),
            r[i].subst.end(),
            subst.begin(),
            subst.end(),
            [&](auto& lhs, auto& rhs) {
              return subtype(lhs.second, rhs.second) &&
                subtype(rhs.second, lhs.second);
            }))
      {
        break;
      }
    }

    // Create a new reification with these substitutions.
    if (i == r.size())
    {
      r.emplace_back(this, def, i, subst);
      fresh = true;
    }

    if (enqueue && r[i].instantiate())
      wl.push({def, i});

    return {r[i], fresh};
  };

  Reification& Reifications::get_reification(Node tn)
  {
    // Look down a reified type name and return the reification.
    assert(tn == TypeNameReified);
    Node def = top;

    for (auto& ident : *(tn->front()))
    {
      auto defs = def->lookdown(ident->location());
      assert(defs.size() == 1);
      def = defs.front();
    }

    return map.at(def).at(parse_int(tn->back()));
  }

  std::pair<Node, Subst> Reifications::get_def_subst(Node type)
  {
    // Return the type with no substitution map if this isn't a reified type
    // name.
    if (type != TypeNameReified)
      return {type, {}};

    auto& r = get_reification(type);
    return {r.def, r.subst_orig};
  }

  void Reifications::add_lookup(Node lookup)
  {
    assert(lookup == Lookup);
    auto& l = (lookup / Lhs) == Lhs ? lhs_lookups : rhs_lookups;
    auto& v = l[(lookup / Ident)->location()][parse_int(lookup / Int)];
    Node ta = lookup / TypeArgs;
    auto count = ta->size();
    size_t i = 0;
    Node method_id;

    for (; i < v.size(); i++)
    {
      auto& v_ta = v[i].first;
      method_id = v[i].second;

      if (v_ta->size() != count)
        continue;

      bool found = true;

      for (size_t j = 0; j < count; j++)
      {
        if (
          !subtype(v_ta->at(j), ta->at(j)) || !subtype(ta->at(j), v_ta->at(j)))
        {
          found = false;
          break;
        }
      }

      if (found)
        break;
    }

    if (i == v.size())
    {
      auto id = std::format(
        "{}.{}{}[{}]",
        (lookup / Ident)->location().view(),
        (lookup / Int)->location().view(),
        (lookup / Lhs) == Lhs ? ".ref" : "",
        i);

      method_id = MethodId ^ id;
      v.emplace_back(ta, method_id);
      wl.push({lookup, i});
    }

    lookup << clone(method_id);
  }
}
