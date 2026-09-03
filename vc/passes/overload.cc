#include "../lang.h"

namespace vc
{
  namespace
  {
    struct OverloadCandidate
    {
      Node def;
      Node name;
    };

    Node make_normalized_candidate_name(const Node& name, Node def)
    {
      auto result = clone(name);
      auto elem = result->back();
      auto typeargs = elem / TypeArgs;
      auto typeparams = def / TypeParams;

      if (typeargs->empty() && !typeparams->empty())
      {
        elem->replace(typeargs, unknown_typeargs(typeparams));
      }
      else
      {
        Node normalized = TypeArgs;

        for (auto& typearg : *typeargs)
        {
          if (typearg == Type && typearg->front() == TypeVar)
            normalized << (Type << Unknown);
          else
            normalized << clone(typearg);
        }

        elem->replace(typeargs, normalized);
      }

      return result;
    }

    std::vector<OverloadCandidate>
    find_overload_candidates(Node top, Node name, size_t arity, Node hand)
    {
      std::vector<OverloadCandidate> result;
      auto defs = find_func_defs(top, name);

      for (auto& def : defs)
      {
        if (
          (def / Params)->size() != arity ||
          (def / Lhs)->type() != hand->type())
          continue;

        result.push_back({def, make_normalized_candidate_name(name, def)});
      }

      if (!result.empty() || hand->type() != Rhs)
        return result;

      for (auto& def : defs)
      {
        if (
          (def / Params)->size() != arity || (def / Lhs)->type() != Once)
          continue;

        result.push_back({def, make_normalized_candidate_name(name, def)});
      }

      return result;
    }
  }

  PassDef overload()
  {
    PassDef p{"overload", wfPassFlatANF, dir::once, {}};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        if (node != Call)
          return true;

        auto name = node / FuncName;
        auto matches = find_overload_candidates(
          top, name, (node / Args)->size(), node / Lhs);

        // Calls through a type parameter cannot be resolved until reification.
        if (matches.empty())
          return false;

        if (matches.size() > 1)
        {
          Node error = err(node, "Ambiguous function overload");

          for (auto& match : matches)
            error << errmsg("Candidate:") << errloc(match.def / Ident);

          node->parent()->replace(node, error);
          return false;
        }

        node->replace(name, matches.front().name);
        return false;
      });

      return 0;
    });

    return p;
  }
}
