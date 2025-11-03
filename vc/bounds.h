#pragma once

#include "lang.h"

namespace vc
{
  inline const auto ret_loc = Location("$return");

  struct UnionFind
  {
    std::map<Location, Location> parent;
    std::map<Location, size_t> rank;

    Location find(Location loc)
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

    void unite(Node& a, Node& b)
    {
      assert(a->in({Ident, LocalId, TypeVar}));
      assert(b->in({Ident, LocalId, TypeVar}));
      unite(a->location(), b->location());
    }

    void unite_ret(Node& a)
    {
      assert(a->in({Ident, LocalId, TypeVar}));
      unite(a->location(), ret_loc);
    }

    void unite(Location a, Location b)
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

    bool connected(Location a, Location b)
    {
      return find(a) == find(b);
    }

    void merge(const UnionFind& that)
    {
      for (auto& kv : that.parent)
        unite(kv.first, kv.second);
    }
  };

  struct Bounds
  {
    Nodes lower;
    Nodes upper;

    void assign(Node type)
    {
      if (type == Type)
        type = type->front();

      lower.push_back(type);
    }

    void use(Node type)
    {
      if (type == Type)
        type = type->front();

      upper.push_back(type);
    }

    void merge(const Bounds& that)
    {
      lower.insert(lower.end(), that.lower.begin(), that.lower.end());
      upper.insert(upper.end(), that.upper.begin(), that.upper.end());
    }
  };

  struct BoundsMap
  {
    std::map<Location, Bounds> map;

    Bounds& operator[](Node& node)
    {
      assert(node->in({Ident, LocalId, TypeVar}));
      return map[node->location()];
    }

    Bounds& ret()
    {
      return map[ret_loc];
    }

    void merge(const BoundsMap& that)
    {
      for (auto& kv : that.map)
        map[kv.first].merge(kv.second);
    }
  };
}
