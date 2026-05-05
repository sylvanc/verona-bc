#pragma once

#include "lang.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vc
{
  // A constraint store over identified TypeVars (Phase 1+2 gave each
  // TypeVar AST node a unique Location). Encodes:
  //   - equivalence (unification) via union-find on TypeVar IDs;
  //   - subtype lower bounds (T <: alpha) and upper bounds (alpha <: T);
  //   - bind(alpha, T) is shorthand for add_lower + add_upper.
  //
  // TypeVar leaves themselves can appear in upper/lower bound lists
  // (encoding alpha' <: alpha), canonicalized via root() at solve time.
  //
  // Mode:
  //   - Emit: constraint emission may mutate the store. Bumps the
  //     generation counter on every mutation, invalidating solve_cache.
  //   - Query: read-only. Subtype rules over TypeVars return
  //     "inconclusive" rather than mutating. Used at reify-time and
  //     for error-path subtype calls so the cache stays warm after
  //     fixed-point.
  class TypeVarStore
  {
  public:
    enum class Mode
    {
      Emit,
      Query,
    };

    // Bound types stored in the store are unwrapped (i.e., the inner
    // type expression — TypeName, Union, Isect, primitive, TypeVar,
    // etc.). substitute() takes/returns Type-wrapped nodes.

    // Lookup or allocate a dense ID for a TypeVar Location.
    uint32_t intern(const Location& loc);

    // Find with path compression.
    uint32_t root(uint32_t id);

    // Union-find merge. Picks one root and reparents the other. Bound
    // sets are merged. Calls bind() if either side was bound.
    void unify(uint32_t a, uint32_t b);

    // alpha := T. T is an unwrapped type expression. Triggers
    // structural decomposition for compound T (ref/tuple/nominal-
    // generic/primitive). Calls occurs check.
    void bind(uint32_t id, Node concrete);

    // alpha <: T (upper) or T <: alpha (lower). T may be a TypeVar
    // leaf to encode alpha <: alpha' or alpha' <: alpha. Dedup'd via
    // structural-hash side-set for amortized O(1).
    void add_upper(uint32_t id, Node t);
    void add_lower(uint32_t id, Node t);

    // Resolve alpha to its representative type. May contain TypeVar
    // leaves transitively. Returns the empty Union (bottom) when no
    // observation exists (zero-evidence parametric returns).
    // Caches per-generation; cache invalidated on any mutation.
    Node solve(uint32_t id);

    // Walk a Type-wrapped tree, replacing TypeVar leaves with their
    // solve() result. has_typevar_out is set true if the substituted
    // result still contains free TypeVar leaves (used by IR-boundary
    // scan and short-circuit-clone optimization).
    Node substitute(const Node& type_node, bool& has_typevar_out);

    // Diagnostic: list TypeVar Locations whose root is unbound and
    // has no usable lower/upper bound (i.e., would be solved to bottom).
    std::vector<Location> free_typevars();

    // Mode control.
    void set_mode(Mode m)
    {
      mode = m;
    }

    Mode get_mode() const
    {
      return mode;
    }

    // Generation counter; bumped on every mutation. Used by callers
    // (e.g., apply_subst) to assert the store is "frozen" across
    // their walk.
    uint64_t generation() const
    {
      return gen;
    }

    // True if the location was already interned; useful for callers
    // that want to avoid creating IDs for one-off queries.
    bool has(const Location& loc) const
    {
      return loc_to_id.find(loc) != loc_to_id.end();
    }

    // Total number of TypeVars known to the store (including non-roots).
    size_t size() const
    {
      return id_to_loc.size();
    }

    // Total number of distinct equivalence classes (root count). For
    // diagnostics.
    size_t class_count();

  private:
    struct LocationHash
    {
      size_t operator()(const Location& loc) const
      {
        return std::hash<std::string_view>{}(loc.view());
      }
    };

    Mode mode = Mode::Emit;

    // Dense ID assignment per first-seen Location.
    std::unordered_map<Location, uint32_t, LocationHash> loc_to_id;
    std::vector<Location> id_to_loc;

    // Union-find: uf_parent[i] == i for roots.
    std::vector<uint32_t> uf_parent;

    // Per-root state. Indexed by root id (use root() before access).
    std::vector<Node> bound;
    std::vector<std::vector<Node>> lower;
    std::vector<std::vector<Node>> upper;

    // Auxiliary structural-hash sets for amortized-O(1) bound dedup.
    std::vector<std::unordered_set<size_t>> lower_hash;
    std::vector<std::unordered_set<size_t>> upper_hash;

    // Mutation counter. Bumped on bind/add_upper/add_lower/unify.
    uint64_t gen = 0;

    // Memoized solve result per root id. Entry is valid iff its
    // generation matches `gen`.
    std::vector<std::pair<uint64_t, Node>> solve_cache;

    // Grow per-root vectors to accommodate id.
    void grow_to(uint32_t id);

    // Bump generation counter on mutation.
    void mutate();

    // Occurs check: does TypeVar with root r appear inside t?
    bool occurs(uint32_t r, const Node& t);

    // Structural composition. Inputs are unwrapped type expressions.
    // lub returns Union(...) (or single if all equal); glb returns
    // Isect(...). Both deduplicate structurally.
    Node lub(const std::vector<Node>& ts);
    Node glb(const std::vector<Node>& ts);

    // Structural hash and equality on unwrapped type expressions.
    size_t structural_hash(const Node& t) const;
    bool structural_equal(const Node& a, const Node& b) const;

    // Structural decomposition during bind/unify. T is an unwrapped
    // type expression. Emits sub-constraints per decomposition table.
    void decompose_unify(uint32_t id, const Node& t);
  };
}
