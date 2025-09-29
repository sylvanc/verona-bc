#pragma once

#include "reification.h"

#include <stack>

namespace vc
{
  // Lookup by name and arity and get a vector of invariant type arguments.
  using Lookups =
    std::map<Location, std::map<size_t, std::vector<std::pair<Node, Node>>>>;

  struct Reifications
  {
    std::shared_ptr<Bytecode> state;

    // Keep the top of the AST to help resolve names.
    Node top;
    Node builtin;

    // A map of definition site to all reifications of that definition.
    NodeMap<std::vector<Reification>> map;

    // A work list of definition site and index into the reification list.
    std::stack<std::pair<Node, size_t>> wl;

    // LHS and RHS lookups.
    Lookups lhs_lookups;
    Lookups rhs_lookups;

    void start(Node t);
    void run();

    std::pair<Reification&, bool> schedule(Node def, Subst subst, bool enqueue);
    Reification& get_reification(Node type);
    std::pair<Node, Subst> get_def_subst(Node type);
    void add_lookup(Node lookup);
  };
}
