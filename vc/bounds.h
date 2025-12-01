#pragma once

#include "lang.h"

namespace vc
{
  struct UnionFind
  {
    std::map<Location, Location> parent;
    std::map<Location, size_t> rank;

    Location find(const Location& loc)
    {
      if (parent.find(loc) == parent.end())
      {
        parent[loc] = loc;
        rank[loc] = 0;
        return loc;
      }

      if (parent[loc] != loc)
        parent[loc] = find(parent[loc]);

      return parent[loc];
    }

    void unite(const Node& a, const Node& b)
    {
      assert(a->in({Ident, LocalId, TypeVar}));
      assert(b->in({Ident, LocalId, TypeVar}));
      unite(a->location(), b->location());
    }

    void unite(const Location& a, const Location& b)
    {
      Location root_a = find(a);
      Location root_b = find(b);

      if (root_a == root_b)
        return;

      if (rank[root_a] < rank[root_b])
      {
        parent[root_a] = root_b;
      }
      else if (rank[root_a] > rank[root_b])
      {
        parent[root_b] = root_a;
      }
      else
      {
        parent[root_b] = root_a;
        rank[root_a]++;
      }
    }

    bool connected(const Location& a, const Location& b)
    {
      return find(a) == find(b);
    }

    std::vector<Location> group(const Node& node)
    {
      assert(node->in({Ident, LocalId, TypeVar}));
      return group(node->location());
    }

    std::vector<Location> group(const Location& a)
    {
      Location root = find(a);
      std::vector<Location> v;

      for (auto& kv : parent)
      {
        if (find(kv.first) == root)
          v.push_back(kv.first);
      }

      return v;
    }

    void merge(const UnionFind& that)
    {
      for (auto& kv : that.parent)
        unite(kv.first, kv.second);
    }
  };

  struct Bounds
  {
    NodeSet lower;
    NodeSet upper;

    void assign(const Node& type)
    {
      if (type == Type)
        assign(type->front());
      else
        lower.insert(type);
    }

    void use(const Node& type)
    {
      if (type == Type)
        use(type->front());
      else
        upper.insert(type);
    }

    void merge(Bounds& that)
    {
      lower.merge(that.lower);
      upper.merge(that.upper);
    }
  };

  struct BoundsMap
  {
    std::map<Location, Bounds> map;

    Bounds& operator[](const Location& loc)
    {
      return map[loc];
    }

    Bounds& operator[](const Node& node)
    {
      assert(node->in({Ident, LocalId, TypeVar}));
      return map[node->location()];
    }

    void merge(BoundsMap& that)
    {
      for (auto& kv : that.map)
        map[kv.first].merge(kv.second);
    }
  };
}
