#pragma once

#include "lang.h"

namespace vc
{
  struct Reifications;
  struct Reification;
  using Subst = NodeMap<Node>;

  enum ReifyResult
  {
    Ok,
    Delay,
    Fail,
  };

  struct PathReification
  {
    // Allows scheduling new reifications, and looking up reified names.
    Reifications* rs;

    // The TypeName or QName being reified.
    Node path;

    // For a QName, the LHS/RHS and arity.
    Node ref;
    size_t args;

    // The current definition the path is relative to.
    Node curr_def;

    // The current type substitutions.
    Subst subst;

    // True if we're emitting errors, false if not. This is false when exploring
    // a `use` include to see if there's a definition available.
    bool errors;

    // The result. This is a reified_name or a TypeAlias reification (for a
    // TypeName) or a FunctionId (for a QName).
    Node result;

    PathReification(Reifications* rs, Subst& subst, Node path);
    PathReification(
      Reifications* rs, Subst& subst, Node path, Node ref, Node args);

    ReifyResult run();
    ReifyResult do_path(size_t i);
    ReifyResult do_element(Node elem, bool last);
    ReifyResult do_classdef(Node& def, Node& typeargs, bool scope);
    ReifyResult do_typealias(Node& def, Node& typeargs, bool scope);
    ReifyResult do_typeargs(Node& def, Node& typeargs, bool scope);
    ReifyResult default_typeargs(Node typeparams, Node typeargs = {});
    ReifyResult do_schedule(Node& def, bool enqueue);
  };
}
