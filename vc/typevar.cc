#include "typevar.h"

namespace vc
{
  namespace
  {
    // Recursive structural-hash helper. Mirrors same_type_tree but
    // returns a hash. Uses location().view() for Ident and TypeVar
    // (the only Location-significant tokens for typing purposes).
    size_t hash_combine(size_t seed, size_t v)
    {
      return seed ^ (v + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }

    size_t hash_node_recursive(const Node& n)
    {
      if (!n)
        return 0;

      size_t h = std::hash<std::string_view>{}(n->type().str());

      if (n == Ident || n == TypeVar)
        h = hash_combine(h, std::hash<std::string_view>{}(n->location().view()));

      for (auto& child : *n)
        h = hash_combine(h, hash_node_recursive(child));

      return h;
    }

    // Structural equality on unwrapped type expressions. Mirrors
    // same_type_tree (kept local because vc/typevar.cc cannot depend
    // on infer.cc).
    bool same_tree(const Node& l, const Node& r)
    {
      if (l == r)
        return true;
      if (!l || !r)
        return false;
      if (l->type() != r->type())
        return false;
      if (l->size() != r->size())
        return false;
      if (l == Ident || l == TypeVar)
        return l->location().view() == r->location().view();
      for (size_t i = 0; i < l->size(); i++)
        if (!same_tree(l->at(i), r->at(i)))
          return false;
      return true;
    }
  }

  uint32_t TypeVarStore::intern(const Location& loc)
  {
    auto it = loc_to_id.find(loc);
    if (it != loc_to_id.end())
      return it->second;

    auto id = static_cast<uint32_t>(id_to_loc.size());
    loc_to_id.emplace(loc, id);
    id_to_loc.push_back(loc);
    grow_to(id);
    return id;
  }

  void TypeVarStore::grow_to(uint32_t id)
  {
    auto needed = static_cast<size_t>(id) + 1;
    if (uf_parent.size() < needed)
    {
      auto old = uf_parent.size();
      uf_parent.resize(needed);
      bound.resize(needed);
      lower.resize(needed);
      upper.resize(needed);
      lower_hash.resize(needed);
      upper_hash.resize(needed);
      solve_cache.resize(needed);

      for (size_t i = old; i < needed; i++)
        uf_parent[i] = static_cast<uint32_t>(i);
    }
  }

  uint32_t TypeVarStore::root(uint32_t id)
  {
    auto r = id;
    while (uf_parent[r] != r)
      r = uf_parent[r];

    auto cur = id;
    while (uf_parent[cur] != r)
    {
      auto next = uf_parent[cur];
      uf_parent[cur] = r;
      cur = next;
    }
    return r;
  }

  void TypeVarStore::mutate()
  {
    gen++;
  }

  size_t TypeVarStore::structural_hash(const Node& t) const
  {
    return hash_node_recursive(t);
  }

  bool TypeVarStore::structural_equal(const Node& a, const Node& b) const
  {
    return same_tree(a, b);
  }

  bool TypeVarStore::occurs(uint32_t r, const Node& t)
  {
    if (!t)
      return false;

    if (t == TypeVar)
    {
      auto& loc = t->location();
      auto it = loc_to_id.find(loc);
      if (it == loc_to_id.end())
        return false;
      return root(it->second) == r;
    }

    for (auto& child : *t)
      if (occurs(r, child))
        return true;

    return false;
  }

  void TypeVarStore::add_lower(uint32_t id, Node t)
  {
    auto r = root(id);
    auto h = structural_hash(t);
    auto& hset = lower_hash[r];
    if (hset.find(h) != hset.end())
    {
      // Possible duplicate; verify structurally before skipping.
      for (auto& existing : lower[r])
        if (structural_equal(existing, t))
          return;
    }
    hset.insert(h);
    lower[r].push_back(t);
    mutate();
  }

  void TypeVarStore::add_upper(uint32_t id, Node t)
  {
    auto r = root(id);
    auto h = structural_hash(t);
    auto& hset = upper_hash[r];
    if (hset.find(h) != hset.end())
    {
      for (auto& existing : upper[r])
        if (structural_equal(existing, t))
          return;
    }
    hset.insert(h);
    upper[r].push_back(t);
    mutate();
  }

  void TypeVarStore::bind(uint32_t id, Node concrete)
  {
    auto r = root(id);

    // Occurs check: T must not transitively reference our own root.
    if (occurs(r, concrete))
    {
      // RecursiveTypeError: emit to caller via free_typevars or a
      // separate error path. For now, leave the root unbound and
      // let solve()/IR-boundary scan surface it.
      // TODO(typevar): structured error reporting hook.
      return;
    }

    if (bound[r])
    {
      // Already bound. If structurally equal, no-op.
      if (structural_equal(bound[r], concrete))
        return;

      // Conflicting bind. Decompose: maybe one side is a structural
      // type whose components unify with the other. For now, we
      // surface this via the lo<:hi check in solve() — both bindings
      // get added as upper+lower bounds.
      add_upper(id, concrete);
      add_lower(id, concrete);
      return;
    }

    bound[r] = concrete;
    add_upper(id, concrete);
    add_lower(id, concrete);
    mutate();
  }

  void TypeVarStore::unify(uint32_t a, uint32_t b)
  {
    auto ra = root(a);
    auto rb = root(b);
    if (ra == rb)
      return;

    // Pick the smaller-id root as the representative (deterministic
    // and gives a stable ordering for diagnostics).
    auto keep = (ra < rb) ? ra : rb;
    auto drop = (ra < rb) ? rb : ra;

    uf_parent[drop] = keep;

    // Merge bound: if both bound, keep one and add the other as
    // upper+lower (lo<:hi check at solve will catch any conflict).
    if (bound[drop])
    {
      if (bound[keep])
      {
        if (!structural_equal(bound[keep], bound[drop]))
        {
          // Conflict: record both as upper+lower of keep so the
          // solve()-time check surfaces it.
          add_upper(keep, bound[drop]);
          add_lower(keep, bound[drop]);
        }
      }
      else
      {
        bound[keep] = bound[drop];
      }
    }

    // Merge bound lists (with dedup against keep's existing).
    for (auto& t : lower[drop])
      add_lower(keep, t);
    for (auto& t : upper[drop])
      add_upper(keep, t);

    // Clear drop's per-root state.
    bound[drop] = {};
    lower[drop].clear();
    upper[drop].clear();
    lower_hash[drop].clear();
    upper_hash[drop].clear();

    mutate();
  }

  Node TypeVarStore::lub(const std::vector<Node>& ts)
  {
    if (ts.empty())
      return {};
    if (ts.size() == 1)
      return ts.front();

    // Collect distinct members (flatten Union arms).
    std::vector<Node> members;
    auto add = [&](const Node& m) {
      for (auto& existing : members)
        if (structural_equal(existing, m))
          return;
      members.push_back(m);
    };

    std::function<void(const Node&)> flatten = [&](const Node& t) {
      if (t == Union)
      {
        for (auto& child : *t)
          flatten(child);
      }
      else
      {
        add(t);
      }
    };

    for (auto& t : ts)
      flatten(t);

    if (members.size() == 1)
      return members.front();

    Node u = Union;
    for (auto& m : members)
      u << clone(m);
    return u;
  }

  Node TypeVarStore::glb(const std::vector<Node>& ts)
  {
    if (ts.empty())
      return {};
    if (ts.size() == 1)
      return ts.front();

    std::vector<Node> members;
    auto add = [&](const Node& m) {
      for (auto& existing : members)
        if (structural_equal(existing, m))
          return;
      members.push_back(m);
    };

    std::function<void(const Node&)> flatten = [&](const Node& t) {
      if (t == Isect)
      {
        for (auto& child : *t)
          flatten(child);
      }
      else
      {
        add(t);
      }
    };

    for (auto& t : ts)
      flatten(t);

    if (members.size() == 1)
      return members.front();

    Node i = Isect;
    for (auto& m : members)
      i << clone(m);
    return i;
  }

  Node TypeVarStore::solve(uint32_t id)
  {
    auto r = root(id);

    auto& cache = solve_cache[r];
    if (cache.first == gen && cache.second)
      return clone(cache.second);

    Node result;

    if (bound[r])
    {
      result = clone(bound[r]);
    }
    else
    {
      // Canonicalize TypeVar leaves in bound lists via root before
      // computing lub/glb. A bound whose root equals r is vacuous
      // (cycle); drop it.
      auto canon = [&](const std::vector<Node>& src) {
        std::vector<Node> out;
        for (auto& t : src)
        {
          if (t == TypeVar)
          {
            auto it = loc_to_id.find(t->location());
            if (it != loc_to_id.end() && root(it->second) == r)
              continue;
          }
          out.push_back(t);
        }
        return out;
      };

      auto lo_list = canon(lower[r]);
      auto hi_list = canon(upper[r]);

      auto lo = lub(lo_list);
      auto hi = glb(hi_list);

      if (lo)
        result = clone(lo);
      else if (hi)
        result = clone(hi);
      else
        result = Union; // bottom (empty Union) — zero-evidence
    }

    cache = {gen, clone(result)};
    return result;
  }

  Node TypeVarStore::substitute(const Node& type_node, bool& has_typevar_out)
  {
    has_typevar_out = false;

    if (!type_node)
      return type_node;

    // Type wrapper: unwrap, recurse, rewrap.
    if (type_node == Type)
    {
      bool inner_has_tv = false;
      auto inner = type_node->front();
      auto sub = substitute(inner, inner_has_tv);
      has_typevar_out = inner_has_tv;
      return Type << (sub ? sub : clone(inner));
    }

    // TypeVar leaf.
    if (type_node == TypeVar)
    {
      auto it = loc_to_id.find(type_node->location());
      if (it == loc_to_id.end())
      {
        has_typevar_out = true;
        return clone(type_node);
      }
      auto solved = solve(it->second);
      // Recursively substitute through the result (bound types may
      // themselves contain TypeVars).
      bool inner_has_tv = false;
      auto sub = substitute(solved, inner_has_tv);
      has_typevar_out = inner_has_tv;
      return sub;
    }

    // Compound: recurse into children. If no child contains TypeVar,
    // return original (skip clone).
    bool any_tv = false;
    bool any_changed = false;
    Nodes new_children;
    new_children.reserve(type_node->size());

    for (auto& child : *type_node)
    {
      bool child_has_tv = false;
      auto sub = substitute(child, child_has_tv);
      if (child_has_tv)
        any_tv = true;
      if (!sub || sub.get() != child.get())
        any_changed = true;
      new_children.push_back(sub ? sub : clone(child));
    }

    has_typevar_out = any_tv;

    if (!any_changed)
      return clone(type_node);

    Node out = NodeDef::create(type_node->type(), type_node->location());
    for (auto& c : new_children)
      out->push_back(c);
    return out;
  }

  std::vector<Location> TypeVarStore::free_typevars()
  {
    std::vector<Location> result;
    for (uint32_t id = 0; id < id_to_loc.size(); id++)
    {
      auto r = root(id);
      if (r != id)
        continue;
      // A root is "free" if it has no bound and no usable bounds.
      if (bound[r])
        continue;
      // If solve would return non-empty (lo or hi), it's not free.
      auto solved = solve(r);
      if (solved && solved == Union && solved->empty())
      {
        // Bottom — counts as free for diagnostic purposes.
        result.push_back(id_to_loc[id]);
      }
    }
    return result;
  }

  size_t TypeVarStore::class_count()
  {
    size_t n = 0;
    for (uint32_t id = 0; id < id_to_loc.size(); id++)
      if (root(id) == id)
        n++;
    return n;
  }

  void TypeVarStore::decompose_unify(uint32_t /*id*/, const Node& /*t*/)
  {
    // Phase 4 will fill this in (decomposition during bind/unify per
    // the variance table). For now, bind() handles concrete leaves
    // directly without decomposing.
    // TODO(phase4): implement structural decomposition.
  }
}
