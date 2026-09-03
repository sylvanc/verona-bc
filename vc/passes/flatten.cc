#include "../lang.h"

#include <cstdlib>
#include <format>
#include <set>
#include <string>

namespace vc
{
  namespace
  {
    using TypeParamNames = NodeMap<Location>;

    TypeParamNames
    assign_flattened_typeparam_names(const Node& cls, const Nodes& path)
    {
      TypeParamNames names;
      std::set<std::string> used;

      // Function and alias TypeParams retain their source names. Reserve those
      // names so captured class TypeParams can always be referenced bare.
      (cls / ClassBody)->traverse([&](auto node) {
        if (node == ClassDef)
          return false;

        if (node->in({Function, TypeAlias}))
        {
          for (auto& tp : *(node / TypeParams))
          {
            auto name = std::string((tp / Ident)->location().view());
            used.insert(name);
            names[tp] = Location(name);
          }
        }

        return true;
      });

      // Preserve the innermost class binder spelling. Freshen outer binders
      // that would otherwise be shadowed in the flattened class.
      for (size_t i = path.size(); i > 0; i--)
      {
        auto cls_index = i - 1;

        for (auto& tp : *(path[cls_index] / TypeParams))
        {
          auto source_name = std::string((tp / Ident)->location().view());
          auto name = source_name;
          size_t suffix = 1;

          while (!used.insert(name).second)
            name = std::format("{}${}", source_name, suffix++);

          names[tp] = Location(name);
        }
      }

      return names;
    }

    void collect_typeparam_refs(
      const Node& top,
      const Node& source,
      const Node& copy,
      const TypeParamNames& names,
      std::vector<std::pair<Node, Location>>& refs)
    {
      assert(source->type() == copy->type());
      assert(source->size() == copy->size());

      if (
        (source == TypeName) ||
        ((source == FuncName) && (source->size() > 1)))
      {
        auto def = find_typeparam_def(top, source);
        auto find = names.find(def);

        if (find != names.end())
          refs.emplace_back(copy, find->second);
      }

      auto source_it = source->begin();
      auto copy_it = copy->begin();

      for (; source_it != source->end(); ++source_it, ++copy_it)
      {
        collect_typeparam_refs(
          top, *source_it, *copy_it, names, refs);
      }
    }

    Node clone_rewrite_typeparams(
      const Node& top, const Node& source, const TypeParamNames& names)
    {
      auto copy = clone(source);
      std::vector<std::pair<Node, Location>> refs;
      collect_typeparam_refs(top, source, copy, names, refs);

      for (auto it = refs.rbegin(); it != refs.rend(); ++it)
      {
        auto& [ref, name] = *it;
        Node replacement = (ref == TypeName) ? TypeName : FuncName;
        replacement << (NameElement << (Ident ^ name) << TypeArgs);

        if (ref == FuncName)
          replacement << clone(ref->back());

        ref->parent()->replace(ref, replacement);
      }

      return copy;
    }

    Node
    make_flat_class_typeparams(const Nodes& path, const TypeParamNames& names)
    {
      Node result = TypeParams;

      for (auto& cls : path)
      {
        for (auto& tp : *(cls / TypeParams))
        {
          auto name = names.at(tp);
          result << (TypeParam << (Ident ^ name));
        }
      }

      return result;
    }

    Node make_flat_class_path(
      const Node& top, const Nodes& path, const TypeParamNames& names)
    {
      Node result = ClassPath;
      Nodes prefix;

      for (auto& cls : path)
      {
        prefix.push_back(cls);
        Node source_typeparams = SourceTypeParams;
        Node args = TypeArgs;

        for (auto& tp : *(cls / TypeParams))
        {
          auto name = names.at(tp);
          source_typeparams << clone(tp / Ident);
          args
            << (Type
                << (TypeName
                    << (NameElement << (Ident ^ name) << TypeArgs)));
        }

        result
          << (ClassPathElement
              << (DefId ^ encode_class_path_id(prefix)) << clone(cls / Ident)
              << source_typeparams << args
              << clone_rewrite_typeparams(top, cls / Where, names));
      }

      return result;
    }

    void flatten_class(
      const Node& cls,
      Nodes& path,
      Node& flat_top,
      std::set<std::string>& ids)
    {
      path.push_back(cls);

      auto id = encode_class_path_id(path);
      auto [_, inserted] = ids.insert(id);
      if (!inserted)
      {
        flat_top << err(cls, "Duplicate flat class identifier");
        path.pop_back();
        return;
      }

      Node body = ClassBody;
      Node body_order = ClassBodyOrder;
      Nodes nested;
      auto top = cls->parent(Top);
      auto names = assign_flattened_typeparam_names(cls, path);

      for (auto& child : *(cls / ClassBody))
      {
        if (child == ClassDef)
        {
          nested.push_back(child);
          auto child_path = path;
          child_path.push_back(child);
          body_order << (NestedClassId ^ encode_class_path_id(child_path));
        }
        else
        {
          body << clone_rewrite_typeparams(top, child, names);
          body_order << BodyMember;
        }
      }

      Node flat_class = FlatClass
        << clone(cls / Shape) << (DefId ^ id) << clone(cls / Ident)
        << make_flat_class_typeparams(path, names)
        << make_flat_class_path(top, path, names) << body_order << body;
      flat_top << flat_class;

      for (auto& child : nested)
        flatten_class(child, path, flat_top, ids);

      path.pop_back();
    }

    Node make_flat_tree(const Node& top)
    {
      Node flat_top = Top;
      Nodes path;
      std::set<std::string> ids;

      for (auto& child : *top)
      {
        if (child == ClassDef)
          flatten_class(child, path, flat_top, ids);
        else
          flat_top << clone(child);
      }

      return flat_top;
    }

  }

  PassDef flatten()
  {
    PassDef p{"flatten", wfPassFlatten, dir::once, {}};

    p.pre([](auto top) {
      WFContext context(wfPassFlatten);
      auto flat_top = make_flat_tree(top);

      if (
        !wfPassFlatten.build_st(flat_top) ||
        !wfPassFlatten.check(flat_top))
      {
        top->erase(top->begin(), top->end());
        top << err("Internal compiler error: flatten produced malformed output");
        return 0;
      }

      top->erase(top->begin(), top->end());
      top << *flat_top;
      return 0;
    });

    return p;
  }
}
