#pragma once

#include "bounds.h"
#include "pathreification.h"

namespace vc
{
  struct Reification
  {
    // Keep a pointer to all Reifications. This is used to schedule new
    // reifications and to look up reified names.
    Reifications* rs;

    // This is the original definition.
    Node def;

    // This is the reification index.
    size_t index;

    // This is a map of TypeParam to Type.
    Subst subst;

    // The type substitutions using the TypeParam nodes from def, not instance.
    Subst subst_orig;

    // This is a TypeNameReified for a ClassDef. Anything else is empty.
    Node reified_name;

    // This is a cloned instance of the original ClassDef, TypeAlias, or
    // Function (or a reference to the original if there are no substitutions).
    // This won't exist for a ClassDef unless we call `new` on that class.
    Node instance;

    // Mark this when the instance is finished reifying.
    ReifyResult status;
    size_t delays;

    // These are class reifications that want this reification as a method.
    NodeMap<Node> wants_method;

    // Bounds for type inference.
    BoundsMap bounds;
    UnionFind uf;
    Nodes literals;

    Reification(Reifications* rs, Node def, size_t i, Subst& subst);

    bool instantiate();
    void run();
    void want_method(Node cls, Node method_id);

    void reify_typename(Node node);
    void reify_new(Node node);
    void reify_newarray(Node node);
    void reify_string(Node node);
    Node reify_builtin(const std::string& name);
    void reify_convert(Node node);
    void reify_const(Node node);
    void reify_call(Node node);
    void reify_ffi(Node node);
    void reify_lookup(Node node);
    void reify_ref(Node node);
    void reify_when(Node node);

    void reify_lookups();

    void concrete_types(Nodes types, Node type);
    Nodes literal_types(Nodes types, bool int_lit);
    void pick_literal_type(Node node);
  };
}
