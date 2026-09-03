#include "../lang.h"

#include <cstdlib>
#include <map>
#include <string>

namespace vc
{
  namespace
  {
    Node legacy_typeparam_name(
      const Node& path, const Node& owner, const Node& source_name)
    {
      Node name = TypeName;

      for (auto& segment : *path)
        name << (NameElement << clone(segment / Ident) << TypeArgs);

      if (owner)
        name << (NameElement << clone(owner / Ident) << TypeArgs);

      name << (NameElement << clone(source_name) << TypeArgs);
      return name;
    }

    NodeMap<Node> legacy_typeparam_names(const Node& flat)
    {
      NodeMap<Node> names;
      auto flat_tp = (flat / TypeParams)->begin();
      Node prefix = ClassPath;

      for (auto& segment : *(flat / ClassPath))
      {
        prefix << clone(segment);

        for (auto& source_name : *(segment / SourceTypeParams))
        {
          assert(flat_tp != (flat / TypeParams)->end());
          names[*flat_tp] =
            legacy_typeparam_name(prefix, {}, source_name);
          flat_tp++;
        }
      }

      assert(flat_tp == (flat / TypeParams)->end());

      (flat / ClassBody)->traverse([&](auto node) {
        if (node->in({Function, TypeAlias}))
        {
          for (auto& tp : *(node / TypeParams))
          {
            names[tp] =
              legacy_typeparam_name(flat / ClassPath, node, tp / Ident);
          }
        }

        return true;
      });

      return names;
    }

    void collect_typeparam_refs(
      const Node& flat_top,
      const Node& source,
      const Node& copy,
      const NodeMap<Node>& names,
      std::vector<std::pair<Node, Node>>& refs)
    {
      assert(source->type() == copy->type());
      assert(source->size() == copy->size());

      if (
        (source == TypeName) ||
        ((source == FuncName) && (source->size() > 1)))
      {
        auto def = find_typeparam_def(flat_top, source);
        auto find = names.find(def);

        if (find != names.end())
          refs.emplace_back(copy, find->second);
      }

      auto source_it = source->begin();
      auto copy_it = copy->begin();

      for (; source_it != source->end(); ++source_it, ++copy_it)
      {
        collect_typeparam_refs(
          flat_top, *source_it, *copy_it, names, refs);
      }
    }

    Node clone_legacy_subtree(
      const Node& flat_top,
      const Node& source,
      const NodeMap<Node>& names)
    {
      auto copy = clone(source);
      std::vector<std::pair<Node, Node>> refs;
      collect_typeparam_refs(flat_top, source, copy, names, refs);

      for (auto it = refs.rbegin(); it != refs.rend(); ++it)
      {
        auto& [ref, name] = *it;
        auto replacement = clone(name);

        if (ref == FuncName)
        {
          Node func = FuncName;

          for (auto& elem : *replacement)
            func << clone(elem);

          func << clone(ref->back());
          replacement = func;
        }

        ref->parent()->replace(ref, replacement);
      }

      return copy;
    }

    struct LegacyClass
    {
      Node cls;
      Node members;
      Node order;
    };

    Node reconstruct_legacy(const Node& flat_top)
    {
      std::map<std::string, LegacyClass> classes;
      Nodes flats;

      for (auto& flat : *flat_top)
      {
        assert(flat == FlatClass);
        flats.push_back(flat);

        auto id = std::string((flat / DefId)->location().view());
        auto path = flat / ClassPath;
        auto segment = path->back();
        auto location = (flat / Shape)->location();
        auto names = legacy_typeparam_names(flat);
        Node typeparams = TypeParams ^ location;

        for (auto& source_name : *(segment / SourceTypeParams))
        {
          typeparams
            << ((TypeParam ^ source_name->location())
                << clone(source_name));
        }

        Node cls =
          (ClassDef ^ location)
          << clone(flat / Shape) << clone(flat / Ident) << typeparams
          << clone_legacy_subtree(
               flat_top, segment / Where, names)
          << (ClassBody ^ location);

        if (
          !classes
             .emplace(
               id,
               LegacyClass{
                 cls,
                 clone_legacy_subtree(
                   flat_top, flat / ClassBody, names),
                 clone(flat / ClassBodyOrder)})
             .second)
          std::abort();
      }

      for (auto it = flats.rbegin(); it != flats.rend(); ++it)
      {
        auto flat = *it;
        auto id = std::string((flat / DefId)->location().view());
        auto& legacy = classes.at(id);
        Node body = ClassBody ^ legacy.cls->location();
        auto member = legacy.members->begin();

        for (auto& entry : *legacy.order)
        {
          if (entry == BodyMember)
          {
            assert(member != legacy.members->end());
            body << clone(*member);
            member++;
          }
          else
          {
            assert(entry == NestedClassId);
            auto child_id = std::string(entry->location().view());
            body << classes.at(child_id).cls;
          }
        }

        assert(member == legacy.members->end());
        legacy.cls->replace(legacy.cls / ClassBody, body);
      }

      Node legacy_top = Top;

      for (auto& flat : flats)
      {
        if ((flat / ClassPath)->size() == 1)
        {
          auto id = std::string((flat / DefId)->location().view());
          legacy_top << classes.at(id).cls;
        }
      }

      return legacy_top;
    }
  }

  PassDef unflatten()
  {
    PassDef p{"unflatten", wfPassANF, dir::once, {}};

    p.pre([](auto top) {
      auto legacy_top = reconstruct_legacy(top);
      WFContext context(wfPassANF);

      if (
        !wfPassANF.build_st(legacy_top) ||
        !wfPassANF.check(legacy_top))
        std::abort();

      top->erase(top->begin(), top->end());
      top << *legacy_top;
      return 0;
    });

    return p;
  }
}
