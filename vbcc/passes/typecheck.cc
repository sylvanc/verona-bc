#include "../lang.h"

#include <map>
#include <queue>

namespace vbcc
{
  // Static type checker for the IR. Runs after liveness to catch type errors
  // at compile time that would otherwise be runtime errors in the interpreter.
  //
  // Design:
  // - Per-function forward pass through each label's body.
  // - Tracks the IR type of each register (LocalId -> Node).
  // - Parameters get their declared types; instructions define dst types.
  // - At each instruction, checks operand types are valid.
  // - Reports errors via the standard err() mechanism.
  //
  // What this catches (Category A - per-statement):
  //   BadOperand:       arithmetic/unary on wrong type family
  //   MismatchedTypes:  binary op operands differ
  //   BadRefTarget:     FieldRef on non-object, ArrayRef on non-array
  //   BadLoadTarget:    Load on non-ref
  //   BadConversion:    Convert between incompatible types
  //   BadArgs (arity):  already checked by validids, but types checked here
  //
  // What this catches (Category B - flow-sensitive):
  //   BadType:          arg type vs param type, return type, field store type
  //   MethodNotFound:   Lookup on a class without that method
  //   BadConversion:    Cond on non-Bool
  //
  // What this does NOT catch (Category C - runtime only):
  //   BadArrayIndex, BadStore (regions), BadStoreTarget (immutability),
  //   BadStackEscape, BadAllocTarget, sendability checks.

  // Type category helpers. These mirror the interpreter's type families.
  // For Union types, ALL members must satisfy the predicate.
  // Dyn is conservatively allowed in all checks.
  static bool is_primitive(const Node& t)
  {
    return t->type().in(
      {None,
       Bool,
       I8,
       I16,
       I32,
       I64,
       U8,
       U16,
       U32,
       U64,
       ILong,
       ULong,
       ISize,
       USize,
       F32,
       F64,
       Ptr,
       Callback});
  }

  static bool is_numeric(const Node& t)
  {
    if (t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!is_numeric(child))
          return false;
      return t->size() > 0;
    }
    return t->type().in(
      {Bool,
       I8,
       I16,
       I32,
       I64,
       U8,
       U16,
       U32,
       U64,
       ILong,
       ULong,
       ISize,
       USize,
       F32,
       F64});
  }

  static bool is_float(const Node& t)
  {
    if (t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!is_float(child))
          return false;
      return t->size() > 0;
    }
    return t->type().in({F32, F64});
  }

  static bool is_object_type(const Node& t)
  {
    if (t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!is_object_type(child))
          return false;
      return t->size() > 0;
    }
    return t == ClassId;
  }

  static bool is_array_type(const Node& t)
  {
    if (t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!is_array_type(child))
          return false;
      return t->size() > 0;
    }
    return t == Array || t == TupleType;
  }

  static bool is_cown_type(const Node& t)
  {
    if (t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!is_cown_type(child))
          return false;
      return t->size() > 0;
    }
    return t == Cown;
  }

  static bool is_ref_type(const Node& t)
  {
    if (t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!is_ref_type(child))
          return false;
      return t->size() > 0;
    }
    return t == Ref;
  }

  // Find the first non-Dyn concrete leaf type in a (possibly Union) type.
  // Returns null if t is Dyn, empty Union, or null.
  static Node first_concrete_leaf(const Node& t)
  {
    if (!t || t == Dyn)
      return {};
    if (t == Union)
    {
      for (auto& child : *t)
      {
        auto r = first_concrete_leaf(child);
        if (r)
          return r;
      }
      return {};
    }
    return t;
  }

  // Check that all non-Dyn leaves in t have the given token type.
  static bool all_leaves_are(const Node& t, const Token& expected)
  {
    if (!t || t == Dyn)
      return true;
    if (t == Union)
    {
      for (auto& child : *t)
        if (!all_leaves_are(child, expected))
          return false;
      return true;
    }
    return t->type() == expected;
  }

  // Check if sub is a subtype of super in the IR type system.
  // This mirrors the interpreter's Program::subtype() logic:
  //   - Dyn is top (everything is a subtype of Dyn)
  //   - Dyn is bottom for runtime purposes (Dyn could be anything)
  //   - Union: all members of sub must be subtypes of super
  //   - Array/Cown/Ref: invariant in their element type
  //   - Same primitive/ClassId: equal
  static bool ir_subtype(const Node& sub, const Node& super)
  {
    if (!sub || !super)
      return true; // Unknown type - be conservative

    // Everything is a subtype of Dyn.
    if (super == Dyn)
      return true;

    // Dyn is a subtype of nothing (conservatively - it could be anything).
    // In static checking, we treat Dyn as compatible to avoid false positives.
    if (sub == Dyn)
      return true;

    // TypeId should be resolved before calling ir_subtype.
    // If one still appears, conservatively allow it.
    if (sub == TypeId || super == TypeId)
      return true;

    // Same token type for primitives.
    if (is_primitive(sub) && is_primitive(super))
      return sub->type() == super->type();

    // ClassId: same string value.
    if (sub == ClassId && super == ClassId)
      return sub->location() == super->location();

    // Union sub: all members must be subtypes of super.
    if (sub == Union)
    {
      for (auto& child : *sub)
      {
        if (!ir_subtype(child, super))
          return false;
      }
      return true;
    }

    // Union super: sub must be a subtype of at least one member.
    if (super == Union)
    {
      for (auto& child : *super)
      {
        if (ir_subtype(sub, child))
          return true;
      }
      return false;
    }

    // Array/Cown/Ref: invariant.
    if (sub == Array && super == Array)
      return ir_subtype(sub / Type, super / Type) &&
        ir_subtype(super / Type, sub / Type);

    if (sub == Cown && super == Cown)
      return ir_subtype(sub / Type, super / Type) &&
        ir_subtype(super / Type, sub / Type);

    if (sub == Ref && super == Ref)
      return ir_subtype(sub / Type, super / Type) &&
        ir_subtype(super / Type, sub / Type);

    // TupleType vs TupleType: covariant element-wise, same arity.
    if (sub == TupleType && super == TupleType)
    {
      if (sub->size() != super->size())
        return false;

      for (size_t i = 0; i < sub->size(); i++)
      {
        if (!ir_subtype(sub->at(i), super->at(i)))
          return false;
      }

      return true;
    }

    // TupleType <: Array(T) if all elements <: T.
    if (sub == TupleType && super == Array && super->size() > 0)
    {
      auto elem_type = super->front();

      for (auto& child : *sub)
      {
        if (!ir_subtype(child, elem_type))
          return false;
      }

      return true;
    }

    // ClassId is a subtype of a Union containing that ClassId.
    // (Handled by the Union super case above.)

    // Primitive vs ClassId, or different complex types: not subtypes.
    return false;
  }

  // Get the type of a register from the type environment.
  // Returns null Node if the register type is unknown.
  static Node
  get_type(const std::unordered_map<std::string, Node>& env, const Node& id)
  {
    auto key = std::string(id->location().view());
    auto it = env.find(key);
    if (it != env.end())
      return it->second;
    return {};
  }

  // Set the type of a register in the type environment.
  static void set_type(
    std::unordered_map<std::string, Node>& env,
    const Node& id,
    const Node& type)
  {
    auto key = std::string(id->location().view());
    env[key] = type;
  }

  // Get a human-readable type name for error messages.
  // Recursively formats compound types (Union, Array, Cown, Ref).
  static std::string type_name(const Node& t)
  {
    if (!t)
      return "<unknown>";
    if (is_primitive(t))
      return std::string(t->type().str());
    if (t == ClassId)
      return std::string(t->location().view());
    if (t == TypeId)
      return std::string(t->location().view());
    if (t == Dyn)
      return "dyn";
    if (t == Array)
    {
      if (t->size() > 0)
        return std::format("Array({})", type_name(t / Type));
      return "Array";
    }
    if (t == Cown)
    {
      if (t->size() > 0)
        return std::format("Cown({})", type_name(t / Type));
      return "Cown";
    }
    if (t == Ref)
    {
      if (t->size() > 0)
        return std::format("Ref({})", type_name(t / Type));
      return "Ref";
    }
    if (t == Union)
    {
      std::string result = "Union(";
      bool first = true;
      for (auto& child : *t)
      {
        if (!first)
          result += ", ";
        result += type_name(child);
        first = false;
      }
      result += ")";
      return result;
    }
    if (t == TupleType)
    {
      std::string result = "Tuple(";
      bool first = true;
      for (auto& child : *t)
      {
        if (!first)
          result += ", ";
        result += type_name(child);
        first = false;
      }
      result += ")";
      return result;
    }
    return std::string(t->type().str());
  }

  // Subtract a type from a (possibly union) type.
  // Returns the type with `to_remove` components stripped out.
  static Node type_subtract(const Node& type, const Node& to_remove)
  {
    if (!type || type == Dyn)
      return type ? clone(type) : Node(Dyn);

    if (type == Union)
    {
      Node result = Union;
      for (auto& child : *type)
      {
        // Keep members that don't match to_remove.
        if (!ir_subtype(child, to_remove) || !ir_subtype(to_remove, child))
          result << clone(child);
      }

      if (result->size() == 0)
        return Node(Dyn);
      if (result->size() == 1)
        return clone(result->front());
      return result;
    }

    // Non-union: if it equals to_remove, nothing left.
    if (ir_subtype(type, to_remove) && ir_subtype(to_remove, type))
      return Node(Dyn);
    return clone(type);
  }

  // Merge (union) two types at a control flow join point.
  static Node type_merge(const Node& a, const Node& b)
  {
    if (!a || a == Dyn)
      return Node(Dyn);
    if (!b || b == Dyn)
      return Node(Dyn);

    // If equal, keep one.
    if (ir_subtype(a, b) && ir_subtype(b, a))
      return clone(a);

    // Build union from both sides.
    std::vector<Node> members;
    auto add = [&](const Node& t) {
      if (t == Union)
      {
        for (auto& child : *t)
          members.push_back(child);
      }
      else
      {
        members.push_back(t);
      }
    };
    add(a);
    add(b);

    // Deduplicate.
    std::vector<Node> unique;
    for (auto& m : members)
    {
      bool found = false;
      for (auto& u : unique)
      {
        if (ir_subtype(m, u) && ir_subtype(u, m))
        {
          found = true;
          break;
        }
      }
      if (!found)
        unique.push_back(m);
    }

    if (unique.size() == 1)
      return clone(unique[0]);
    Node result = Union;
    for (auto& u : unique)
      result << clone(u);
    return result;
  }

  PassDef typecheck(std::shared_ptr<Bytecode> state)
  {
    PassDef p{"typecheck", wfIR, dir::topdown | dir::once, {}};

    p.post([state](auto top) {
      // Type environment: register name -> IR type node.
      std::unordered_map<std::string, Node> env;
      Node cur_func;

      // Track lookup results: dst register -> (src_type, MethodId).
      struct LookupInfo
      {
        Node src_type;
        Node method_id;
      };
      std::unordered_map<std::string, LookupInfo> lookup_info;

      // Collect errors during traversal, apply after.
      // We cannot call replace() during traverse() because it invalidates
      // the parent's child iterators.
      std::vector<std::pair<Node, std::string>> errors;
      bool checking = false;

      auto type_err = [&](const Node& node, const std::string& msg) {
        if (!checking)
          return;
        state->error = true;
        errors.push_back({node, msg});
      };

      // Resolve TypeId to its definition (typically a Union).
      // Type entries in the IR are: Type <<= TypeId * (Type >>= wfType).
      // Recursively resolves through Union, Array, Cown, and Ref.
      std::function<Node(const Node&)> resolve_type =
        [&](const Node& t) -> Node {
        if (!t)
          return t;
        if (t == TypeId)
        {
          for (auto& child : *top)
          {
            if (child == Type && (child / TypeId)->location() == t->location())
              return resolve_type(child / Type);
          }
          return t; // Unresolved TypeId - leave as-is
        }
        if (t == Union)
        {
          // Resolve children, flatten nested unions, and normalize.
          std::vector<Node> flat_children;
          bool changed = false;
          for (auto& child : *t)
          {
            auto resolved = resolve_type(child);
            if (resolved.get() != child.get())
              changed = true;
            // Flatten: if resolved child is a Union, splice its children.
            if (resolved == Union)
            {
              changed = true;
              for (auto& grandchild : *resolved)
                flat_children.push_back(grandchild);
            }
            else
            {
              flat_children.push_back(resolved);
            }
          }
          // Normalize: empty -> Dyn, single -> unwrap, otherwise Union.
          if (flat_children.empty())
            return Node(Dyn);
          if (flat_children.size() == 1)
            return clone(flat_children[0]);
          if (!changed)
            return t;
          auto result = Node(Union);
          for (auto& rc : flat_children)
            result << clone(rc);
          return result;
        }
        if (t->type().in({Array, Cown, Ref}))
        {
          Node inner = t / Type;
          auto resolved = resolve_type(inner);
          if (resolved.get() != inner.get())
          {
            auto result = Node(t->type());
            result << clone(resolved);
            return result;
          }
        }
        if (t == TupleType)
        {
          bool changed = false;
          std::vector<Node> resolved_children;

          for (auto& child : *t)
          {
            auto resolved = resolve_type(child);
            resolved_children.push_back(resolved);

            if (resolved.get() != child.get())
              changed = true;
          }

          if (changed)
          {
            auto result = Node(TupleType);

            for (auto& rc : resolved_children)
              result << clone(rc);

            return result;
          }
        }
        return t;
      };

      // Wrapper around get_type that resolves TypeId.
      auto typed = [&](const Node& id) -> Node {
        return resolve_type(get_type(env, id));
      };

      // Look up a function definition by FunctionId.
      auto find_func = [&](const Node& func_id) -> Node {
        for (auto& child : *top)
        {
          if (
            child == Func &&
            (child / FunctionId)->location() == func_id->location())
            return child;
        }
        return {};
      };

      // Look up a class definition by ClassId.
      auto find_class = [&](const Node& class_id) -> Node {
        for (auto& child : *top)
        {
          if (
            child == Class &&
            (child / ClassId)->location() == class_id->location())
            return child;
        }
        return {};
      };

      // Look up a primitive class by type node.
      auto find_primitive = [&](const Node& type_node) -> Node {
        for (auto& child : *top)
        {
          if (child == Primitive && (child / Type)->type() == type_node->type())
            return child;
        }
        return {};
      };

      // Check if a class/primitive has a given method.
      auto has_method =
        [&](const Node& type_node, const Node& method_id) -> bool {
        Node cls;

        if (type_node == ClassId)
          cls = find_class(type_node);
        else if (is_primitive(type_node))
          cls = find_primitive(type_node);
        else if (type_node == Array || type_node == Cown || type_node == Ref)
        {
          // Complex primitives - find the matching primitive definition.
          for (auto& child : *top)
          {
            if (child != Primitive)
              continue;
            auto ptype = child / Type;
            if (ptype->type() == type_node->type())
            {
              cls = child;
              break;
            }
          }
        }

        if (!cls)
          return false;

        for (auto& method : *(cls / Methods))
        {
          if ((method / MethodId)->location() == method_id->location())
            return true;
        }

        return false;
      };

      // Check if ALL members of a union have a method.
      auto union_has_method =
        [&](const Node& type_node, const Node& method_id) -> bool {
        if (type_node == Union)
        {
          for (auto& child : *type_node)
          {
            if (!has_method(child, method_id))
              return false;
          }
          return true;
        }
        return has_method(type_node, method_id);
      };

      // Get the field type for a class.
      auto get_field_type =
        [&](const Node& class_id, const Node& field_id) -> Node {
        auto cls = find_class(class_id);
        if (!cls)
          return {};

        for (auto& field : *(cls / Fields))
        {
          if ((field / FieldId)->location() == field_id->location())
            return field / Type;
        }
        return {};
      };

      // Resolve type of an FFI return from the symbol table.
      auto get_ffi_return_type = [&](const Node& symbol_id) -> Node {
        for (auto& child : *top)
        {
          if (child != Lib)
            continue;
          for (auto& sym : *(child / Symbols))
          {
            if ((sym / SymbolId)->location() == symbol_id->location())
              return sym / Return;
          }
        }
        return Node(Dyn);
      };

      // Lambda to process a single instruction or terminator node.
      // Returns bool for convenience (callers ignore the value).
      auto process_node = [&](const Node& node) -> bool {
        if (node == Error)
        {
          return false;
        }
        else if (node == Const)
        {
          // dst gets the literal type.
          set_type(env, node / LocalId, node / Type);
        }
        else if (node == ConstStr)
        {
          // Strings are arrays of u8.
          auto str_type = Node(Array) << Node(U8);
          set_type(env, node / LocalId, str_type);
        }
        else if (node == Convert)
        {
          auto src_type = typed(node / Rhs);
          auto dst_type_node = node / Type;

          // Both src and target must be primitive types.
          if (
            src_type && !is_numeric(src_type) && src_type != Ptr &&
            src_type != None)
          {
            type_err(
              node,
              std::format(
                "convert: source type '{}' is not a numeric or primitive type",
                type_name(src_type)));
            return true;
          }

          set_type(env, node / LocalId, clone(dst_type_node));
        }
        else if (node->in({Copy, Move}))
        {
          auto src_type = typed(node / Rhs);
          if (src_type)
            set_type(env, node / LocalId, src_type);
          else
            set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node->in({New, Stack}))
        {
          // dst gets the ClassId type. Check arg types vs field types.
          auto class_id = node / ClassId;
          auto args = node / Args;
          auto cls = find_class(class_id);

          if (cls)
          {
            auto fields = cls / Fields;
            auto f_it = fields->begin();
            auto a_it = args->begin();

            while (f_it != fields->end() && a_it != args->end())
            {
              auto field_type = resolve_type((*f_it) / Type);
              auto arg_type = typed((*a_it) / Rhs);

              if (arg_type && !ir_subtype(arg_type, field_type))
              {
                type_err(
                  *a_it,
                  std::format(
                    "new: argument type '{}' is not a subtype of field type "
                    "'{}'",
                    type_name(arg_type),
                    type_name(field_type)));
                return true;
              }

              ++f_it;
              ++a_it;
            }
          }

          set_type(env, node / LocalId, clone(class_id));
        }
        else if (node->in({Heap, Region}))
        {
          // Like New but with extra leading args (region source / region type).
          auto class_id = node / ClassId;
          set_type(env, node / LocalId, clone(class_id));
        }
        else if (node->in({NewArray, StackArray}))
        {
          // dst gets the array type (node / Type is already the full type,
          // e.g. Array(I32) for [i32]).
          set_type(env, node / LocalId, clone(node / Type));
        }
        else if (node->in({NewArrayConst, StackArrayConst}))
        {
          set_type(env, node / LocalId, clone(node / Type));
        }
        else if (node->in(
                   {HeapArray, HeapArrayConst, RegionArray, RegionArrayConst}))
        {
          set_type(env, node / LocalId, clone(node / Type));
        }
        else if (node == RegisterRef)
        {
          auto src_type = typed(node / Rhs);
          if (src_type)
            set_type(env, node / LocalId, Node(Ref) << clone(src_type));
          else
            set_type(env, node / LocalId, Node(Ref) << Dyn);
        }
        else if (node == FieldRef)
        {
          auto src_type = typed(node / Arg / Rhs);

          if (src_type && !is_object_type(src_type))
          {
            type_err(
              node,
              std::format(
                "fieldref: source type '{}' is not an object type",
                type_name(src_type)));
            return true;
          }

          // dst gets Ref of the field's type.
          if (src_type && src_type == ClassId)
          {
            auto field_type =
              resolve_type(get_field_type(src_type, node / FieldId));
            if (field_type)
              set_type(env, node / LocalId, Node(Ref) << clone(field_type));
            else
              set_type(env, node / LocalId, Node(Ref) << Node(Dyn));
          }
          else if (src_type && src_type == Union)
          {
            // Union of ClassIds: build union of Ref(field_type).
            Node union_type = Union;
            bool all_ok = true;
            for (auto& member : *src_type)
            {
              if (member != ClassId)
              {
                all_ok = false;
                break;
              }
              auto ft = resolve_type(get_field_type(member, node / FieldId));
              if (ft)
                union_type << (Node(Ref) << clone(ft));
              else
              {
                all_ok = false;
                break;
              }
            }
            if (all_ok && union_type->size() == 1)
              set_type(env, node / LocalId, clone(union_type->front()));
            else if (all_ok && union_type->size() > 1)
              set_type(env, node / LocalId, union_type);
            else
              set_type(env, node / LocalId, Node(Ref) << Node(Dyn));
          }
          else
          {
            set_type(env, node / LocalId, Node(Ref) << Node(Dyn));
          }
        }
        else if (node->in({ArrayRef, ArrayRefConst}))
        {
          auto src_type = typed(node / Arg / Rhs);

          if (src_type && !is_array_type(src_type))
          {
            type_err(
              node,
              std::format(
                "arrayref: source type '{}' is not an array type",
                type_name(src_type)));
            return true;
          }

          // dst gets Ref of the array's element type.
          if (src_type && src_type == TupleType && node == ArrayRefConst)
          {
            // TupleType is a peer of Array: extract per-element type by
            // constant index.
            auto idx_node = node / Rhs;
            auto idx = from_chars_sep_v<size_t>(idx_node);
            if (idx < src_type->size())
              set_type(
                env,
                node / LocalId,
                Node(Ref) << clone(src_type->at(idx)));
            else
            {
              type_err(
                node,
                std::format(
                  "tuple index {} out of range for tuple with {} elements",
                  idx,
                  src_type->size()));
              return true;
            }
          }
          else if (src_type && src_type == Array && src_type->size() > 0)
            set_type(env, node / LocalId, Node(Ref) << clone(src_type / Type));
          else if (src_type && src_type == Union)
          {
            // Union of Arrays: build union of Ref(element_type).
            Node union_type = Union;
            bool all_ok = true;
            for (auto& member : *src_type)
            {
              if (member == TupleType)
              {
                // TupleType in a union: element type unknown at runtime.
                union_type << (Node(Ref) << Node(Dyn));
              }
              else if (member == Array && member->size() > 0)
              {
                union_type << (Node(Ref) << clone(member / Type));
              }
              else
              {
                all_ok = false;
                break;
              }
            }
            if (all_ok && union_type->size() == 1)
              set_type(env, node / LocalId, clone(union_type->front()));
            else if (all_ok && union_type->size() > 1)
              set_type(env, node / LocalId, union_type);
            else
              set_type(env, node / LocalId, Node(Ref) << Node(Dyn));
          }
          else
            set_type(env, node / LocalId, Node(Ref) << Node(Dyn));
        }
        else if (node == Load)
        {
          auto src_type = typed(node / Rhs);

          if (src_type && !is_ref_type(src_type))
          {
            type_err(
              node,
              std::format(
                "load: source type '{}' is not a reference type",
                type_name(src_type)));
            return true;
          }

          // dst gets the ref's content type.
          if (src_type && src_type == Ref && src_type->size() > 0)
            set_type(env, node / LocalId, clone(src_type / Type));
          else if (src_type && src_type == Union)
          {
            // Union of Refs: build union of content types.
            Node union_type = Union;
            bool all_ok = true;
            for (auto& member : *src_type)
            {
              if (member != Ref || member->size() == 0)
              {
                all_ok = false;
                break;
              }
              union_type << clone(member / Type);
            }
            if (all_ok && union_type->size() == 1)
              set_type(env, node / LocalId, clone(union_type->front()));
            else if (all_ok && union_type->size() > 1)
              set_type(env, node / LocalId, union_type);
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
          else
            set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node == Store)
        {
          auto ref_type = typed(node / Rhs);
          auto val_type = typed(node / Arg / Rhs);

          // The ref's content type determines what can be stored.
          if (ref_type && ref_type == Ref && ref_type->size() > 0 && val_type)
          {
            auto content_type = ref_type / Type;
            if (!ir_subtype(val_type, content_type))
            {
              type_err(
                node,
                std::format(
                  "store: value type '{}' is not a subtype of reference "
                  "content type '{}'",
                  type_name(val_type),
                  type_name(content_type)));
              return true;
            }
          }
          else if (ref_type && ref_type == Union && val_type)
          {
            // Union of Refs: val must be subtype of ALL content types.
            for (auto& member : *ref_type)
            {
              if (member != Ref || member->size() == 0)
                continue;
              auto content_type = member / Type;
              if (!ir_subtype(val_type, content_type))
              {
                type_err(
                  node,
                  std::format(
                    "store: value type '{}' is not a subtype of reference "
                    "content type '{}'",
                    type_name(val_type),
                    type_name(content_type)));
                return true;
              }
            }
          }

          // dst gets the old value (same type as content).
          if (ref_type && ref_type == Ref && ref_type->size() > 0)
            set_type(env, node / LocalId, clone(ref_type / Type));
          else if (ref_type && ref_type == Union)
          {
            // Union of Refs: result is union of content types.
            Node union_type = Union;
            bool all_ok = true;
            for (auto& member : *ref_type)
            {
              if (member != Ref || member->size() == 0)
              {
                all_ok = false;
                break;
              }
              union_type << clone(member / Type);
            }
            if (all_ok && union_type->size() == 1)
              set_type(env, node / LocalId, clone(union_type->front()));
            else if (all_ok && union_type->size() > 1)
              set_type(env, node / LocalId, union_type);
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
          else
            set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node == Lookup)
        {
          auto src_type = typed(node / Rhs);
          auto method_id = node / MethodId;

          // Check that the source type has this method.
          // Skip only for Dyn; union_has_method handles Union recursion.
          if (src_type && src_type != Dyn)
          {
            if (!union_has_method(src_type, method_id))
            {
              type_err(
                node,
                std::format(
                  "lookup: type '{}' does not have method '{}'",
                  type_name(src_type),
                  std::string(method_id->location().view())));
              return true;
            }
          }

          // Track lookup result for CallDyn resolution.
          auto dst_name =
            std::string(node->front()->location().view());
          lookup_info[dst_name] = {src_type, method_id};

          // Lookup produces a function pointer (opaque).
          set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node == FnPointer)
        {
          // Function pointer is opaque.
          set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node == Call)
        {
          auto func_id = node / FunctionId;
          auto args = node / Args;
          auto target_func = find_func(func_id);

          if (target_func)
          {
            // Check arg types vs param types.
            auto params = target_func / Params;
            auto p_it = params->begin();
            auto a_it = args->begin();

            while (p_it != params->end() && a_it != args->end())
            {
              auto param_type = resolve_type((*p_it) / Type);
              auto arg_type = typed((*a_it) / Rhs);

              if (arg_type && !ir_subtype(arg_type, param_type))
              {
                type_err(
                  *a_it,
                  std::format(
                    "call: argument type '{}' is not a subtype of parameter "
                    "type '{}'",
                    type_name(arg_type),
                    type_name(param_type)));
                return true;
              }

              ++p_it;
              ++a_it;
            }

            // dst gets the function's return type.
            set_type(env, node / LocalId, resolve_type(target_func / Type));
          }
          else
          {
            set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node->in({CallDyn, TryCallDyn}))
        {
          // Try to resolve the dynamic call through lookup info.
          auto fn_ptr_name =
            std::string((node / Rhs)->location().view());
          auto it = lookup_info.find(fn_ptr_name);

          if (it != lookup_info.end())
          {
            auto& info = it->second;
            // Resolve method to function: find the class/primitive,
            // look up the MethodId -> FunctionId mapping.
            Node target_func;
            auto resolve_one = [&](const Node& t) -> Node {
              Node cls;
              if (t == ClassId)
                cls = find_class(t);
              else if (is_primitive(t))
                cls = find_primitive(t);
              else if (t->type().in({Array, Cown, Ref}))
              {
                for (auto& child : *top)
                {
                  if (
                    child == Primitive &&
                    (child / Type)->type() == t->type())
                  {
                    cls = child;
                    break;
                  }
                }
              }
              if (!cls)
                return {};
              for (auto& method : *(cls / Methods))
              {
                if (
                  (method / MethodId)->location() ==
                  info.method_id->location())
                  return find_func(method / FunctionId);
              }
              return {};
            };

            // For unions, all members must resolve to the same function.
            if (info.src_type && info.src_type == Union)
            {
              for (auto& member : *info.src_type)
              {
                auto f = resolve_one(member);
                if (f)
                {
                  target_func = f;
                  break;
                }
              }
            }
            else if (info.src_type)
            {
              target_func = resolve_one(info.src_type);
            }

            if (target_func)
            {
              // Check arg types vs param types.
              auto params = target_func / Params;
              auto args = node / Args;
              auto p_it = params->begin();
              auto a_it = args->begin();

              while (p_it != params->end() && a_it != args->end())
              {
                auto param_type = resolve_type((*p_it) / Type);
                auto arg_type = typed((*a_it) / Rhs);

                if (arg_type && !ir_subtype(arg_type, param_type))
                {
                  type_err(
                    *a_it,
                    std::format(
                      "call: argument type '{}' is not a subtype of "
                      "parameter type '{}'",
                      type_name(arg_type),
                      type_name(param_type)));
                  return true;
                }

                ++p_it;
                ++a_it;
              }

              // dst gets the function's return type.
              set_type(
                env, node / LocalId, resolve_type(target_func / Type));
            }
            else
            {
              set_type(env, node / LocalId, Node(Dyn));
            }
          }
          else
          {
            // No lookup info - truly dynamic.
            set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node == FFI)
        {
          auto ret_type = get_ffi_return_type(node / SymbolId);
          set_type(env, node / LocalId, clone(ret_type));
        }
        else if (node->in({When, WhenDyn}))
        {
          // When produces a cown.
          set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node == Typetest)
        {
          // Typetest dst is a boolean.
          set_type(env, node / LocalId, Node(Bool));
        }
        else if (node == Drop)
        {
          // Drop removes a register - nothing to check.
        }
        else if (node->type().in(
                   {Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Min, Max}))
        {
          // Arithmetic/bitwise binops: both operands must be the same numeric
          // type. dst gets that type.
          auto lhs_type = typed(node / Lhs);
          auto rhs_type = typed(node / Rhs);

          if (lhs_type && !is_numeric(lhs_type))
          {
            type_err(
              node,
              std::format(
                "{}: left operand type '{}' is not numeric",
                std::string(node->type().str()),
                type_name(lhs_type)));
            return true;
          }

          if (rhs_type && !is_numeric(rhs_type))
          {
            type_err(
              node,
              std::format(
                "{}: right operand type '{}' is not numeric",
                std::string(node->type().str()),
                type_name(rhs_type)));
            return true;
          }

          // Check all concrete types across both operands match.
          if (lhs_type && rhs_type)
          {
            auto first = first_concrete_leaf(lhs_type);
            if (!first)
              first = first_concrete_leaf(rhs_type);
            if (
              first &&
              !(all_leaves_are(lhs_type, first->type()) &&
                all_leaves_are(rhs_type, first->type())))
            {
              type_err(
                node,
                std::format(
                  "{}: mismatched operand types '{}' and '{}'",
                  std::string(node->type().str()),
                  type_name(lhs_type),
                  type_name(rhs_type)));
              return true;
            }
          }

          {
            auto first = first_concrete_leaf(lhs_type);
            if (!first)
              first = first_concrete_leaf(rhs_type);
            if (first)
              set_type(env, node / LocalId, clone(first));
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node->type().in({Pow, LogBase, Atan2}))
        {
          // Float-only binops.
          auto lhs_type = typed(node / Lhs);
          auto rhs_type = typed(node / Rhs);

          if (lhs_type && !is_float(lhs_type))
          {
            type_err(
              node,
              std::format(
                "{}: left operand type '{}' is not floating-point",
                std::string(node->type().str()),
                type_name(lhs_type)));
            return true;
          }

          if (rhs_type && !is_float(rhs_type))
          {
            type_err(
              node,
              std::format(
                "{}: right operand type '{}' is not floating-point",
                std::string(node->type().str()),
                type_name(rhs_type)));
            return true;
          }

          // Check all concrete types across both operands match.
          if (lhs_type && rhs_type)
          {
            auto first = first_concrete_leaf(lhs_type);
            if (!first)
              first = first_concrete_leaf(rhs_type);
            if (
              first &&
              !(all_leaves_are(lhs_type, first->type()) &&
                all_leaves_are(rhs_type, first->type())))
            {
              type_err(
                node,
                std::format(
                  "{}: mismatched operand types '{}' and '{}'",
                  std::string(node->type().str()),
                  type_name(lhs_type),
                  type_name(rhs_type)));
              return true;
            }
          }

          {
            auto first = first_concrete_leaf(lhs_type);
            if (!first)
              first = first_concrete_leaf(rhs_type);
            if (first)
              set_type(env, node / LocalId, clone(first));
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node->type().in({Eq, Ne, Lt, Le, Gt, Ge}))
        {
          // Comparison binops: both operands same numeric type. dst is Bool.
          auto lhs_type = typed(node / Lhs);
          auto rhs_type = typed(node / Rhs);

          if (lhs_type && !is_numeric(lhs_type))
          {
            type_err(
              node,
              std::format(
                "{}: left operand type '{}' is not numeric",
                std::string(node->type().str()),
                type_name(lhs_type)));
            return true;
          }

          if (rhs_type && !is_numeric(rhs_type))
          {
            type_err(
              node,
              std::format(
                "{}: right operand type '{}' is not numeric",
                std::string(node->type().str()),
                type_name(rhs_type)));
            return true;
          }

          // Check all concrete types across both operands match.
          if (lhs_type && rhs_type)
          {
            auto first = first_concrete_leaf(lhs_type);
            if (!first)
              first = first_concrete_leaf(rhs_type);
            if (
              first &&
              !(all_leaves_are(lhs_type, first->type()) &&
                all_leaves_are(rhs_type, first->type())))
            {
              type_err(
                node,
                std::format(
                  "{}: mismatched operand types '{}' and '{}'",
                  std::string(node->type().str()),
                  type_name(lhs_type),
                  type_name(rhs_type)));
              return true;
            }
          }

          set_type(env, node / LocalId, Node(Bool));
        }
        else if (node->type().in({Neg, Abs}))
        {
          // Signed numeric unops.
          auto src_type = typed(node / Rhs);

          if (src_type && !is_numeric(src_type))
          {
            type_err(
              node,
              std::format(
                "{}: operand type '{}' is not numeric",
                std::string(node->type().str()),
                type_name(src_type)));
            return true;
          }

          {
            auto first = first_concrete_leaf(src_type);
            if (first && all_leaves_are(src_type, first->type()))
              set_type(env, node / LocalId, clone(first));
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node == Not)
        {
          // Bitwise not / boolean not.
          auto src_type = typed(node / Rhs);

          if (src_type && !is_numeric(src_type))
          {
            type_err(
              node,
              std::format(
                "not: operand type '{}' is not numeric", type_name(src_type)));
            return true;
          }

          {
            auto first = first_concrete_leaf(src_type);
            if (first && all_leaves_are(src_type, first->type()))
              set_type(env, node / LocalId, clone(first));
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node->type().in(
                   {Ceil,
                    Floor,
                    Exp,
                    Log,
                    Sqrt,
                    Cbrt,
                    Sin,
                    Cos,
                    Tan,
                    Asin,
                    Acos,
                    Atan,
                    Sinh,
                    Cosh,
                    Tanh,
                    Asinh,
                    Acosh,
                    Atanh}))
        {
          // Float-only unops.
          auto src_type = typed(node / Rhs);

          if (src_type && !is_float(src_type))
          {
            type_err(
              node,
              std::format(
                "{}: operand type '{}' is not floating-point",
                std::string(node->type().str()),
                type_name(src_type)));
            return true;
          }

          {
            auto first = first_concrete_leaf(src_type);
            if (first && all_leaves_are(src_type, first->type()))
              set_type(env, node / LocalId, clone(first));
            else
              set_type(env, node / LocalId, Node(Dyn));
          }
        }
        else if (node->type().in({IsInf, IsNaN}))
        {
          // Float predicate unops: operand must be float, result is Bool.
          auto src_type = typed(node / Rhs);

          if (src_type && !is_float(src_type))
          {
            type_err(
              node,
              std::format(
                "{}: operand type '{}' is not floating-point",
                std::string(node->type().str()),
                type_name(src_type)));
            return true;
          }

          set_type(env, node / LocalId, Node(Bool));
        }
        else if (node == Bits)
        {
          // Bits: numeric -> U64.
          auto src_type = typed(node / Rhs);

          if (src_type && !is_numeric(src_type))
          {
            type_err(
              node,
              std::format(
                "bits: operand type '{}' is not numeric", type_name(src_type)));
            return true;
          }

          set_type(env, node / LocalId, Node(U64));
        }
        else if (node == Len)
        {
          // Len: array -> USize.
          auto src_type = typed(node / Rhs);

          if (src_type && !is_array_type(src_type))
          {
            type_err(
              node,
              std::format(
                "len: operand type '{}' is not an array type",
                type_name(src_type)));
            return true;
          }

          set_type(env, node / LocalId, Node(USize));
        }
        else if (node == MakePtr)
        {
          // MakePtr: anything -> Ptr.
          set_type(env, node / LocalId, Node(Ptr));
        }
        else if (node == Read)
        {
          // Read: cown -> result (same type, but read-only).
          auto src_type = typed(node / Rhs);

          if (src_type && !is_cown_type(src_type))
          {
            type_err(
              node,
              std::format(
                "read: operand type '{}' is not a cown type",
                type_name(src_type)));
            return true;
          }

          // Read gives back the cown type (with readonly semantics at runtime).
          if (src_type)
            set_type(env, node / LocalId, clone(src_type));
          else
            set_type(env, node / LocalId, Node(Dyn));
        }
        else if (node->type().in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
        {
          // Math constants are F64.
          set_type(env, node / LocalId, Node(F64));
        }
        else if (
          node->type().in({AddExternal, RemoveExternal, FreeCallback}))
        {
          // These produce None.
          set_type(env, node / LocalId, Node(None));
        }
        else if (node->type().in({MakeCallback}))
        {
          // MakeCallback produces Callback.
          set_type(env, node / LocalId, Node(Callback));
        }
        else if (node->type().in({CallbackPtr}))
        {
          // CallbackPtr produces Ptr.
          set_type(env, node / LocalId, Node(Ptr));
        }
        else if (node == GetRaise)
        {
          // GetRaise returns a U64 (raw frame ID).
          set_type(env, node / LocalId, Node(U64));
        }
        else if (node == SetRaise)
        {
          // SetRaise returns previous raise target as U64.
          set_type(env, node / LocalId, Node(U64));
        }
        else if (node == Source || node == Offset)
        {
          // Debug info, no type effect.
        }
        else if (node == Return)
        {
          // Check return type.
          if (cur_func)
          {
            auto ret_type = typed(node / LocalId);
            auto func_ret = resolve_type(cur_func / Type);

            if (ret_type && !ir_subtype(ret_type, func_ret))
            {
              type_err(
                node,
                std::format(
                  "return: value type '{}' is not a subtype of function "
                  "return type '{}'",
                  type_name(ret_type),
                  type_name(func_ret)));
              return true;
            }
          }
        }
        else if (node == Raise)
        {
          // Check raised value type against the enclosing function's
          // return type (annotated on the Raise node).
          auto ret_type = typed(node / LocalId);
          auto raise_ret = resolve_type(node / Type);

          if (ret_type && !ir_subtype(ret_type, raise_ret))
          {
            type_err(
              node,
              std::format(
                "raise: value type '{}' is not a subtype of enclosing "
                "function return type '{}'",
                type_name(ret_type),
                type_name(raise_ret)));
            return true;
          }
        }
        else if (node == Cond)
        {
          auto cond_type = typed(node / LocalId);

          if (cond_type && cond_type != Dyn && cond_type != Bool)
          {
            type_err(
              node,
              std::format(
                "cond: condition type '{}' is not bool", type_name(cond_type)));
            return true;
          }
        }

        return true;
      };

      // Per-label type environments for control-flow-sensitive checking.
      // Instead of one flat env per function, we maintain per-label entry/exit
      // envs and merge at join points. This enables TypeCond to narrow the
      // source variable's type on the non-match branch.
      using TypeEnv = std::unordered_map<std::string, Node>;

      for (auto& func_node : *top)
      {
        if (func_node != Func)
          continue;
        if (func_node->get_contains_error())
          continue;

        cur_func = func_node;
        lookup_info.clear();

        auto labels_node = func_node / Labels;
        size_t n_labels = labels_node->size();
        if (n_labels == 0)
          continue;

        // Map func_state label indices to AST label nodes.
        auto& func_state = state->get_func(func_node / FunctionId);
        size_t n_fs_labels = func_state.labels.size();

        std::vector<Node> label_vec(n_fs_labels);
        for (auto& label : *labels_node)
        {
          auto fs_idx = func_state.get_label_id(label / LabelId);
          if (fs_idx)
            label_vec[*fs_idx] = label;
        }

        // Find the start label (first in AST order).
        auto start_opt =
          func_state.get_label_id(labels_node->front() / LabelId);
        size_t start_idx = start_opt ? *start_opt : 0;

        // Per-label exit environments and branch-specific exits for TypeCond.
        std::vector<TypeEnv> exit_envs(n_fs_labels);
        std::vector<std::unordered_map<std::string, TypeEnv>> branch_exits(
          n_fs_labels);
        std::vector<std::string> fingerprints(n_fs_labels);

        // Initialize start label entry env with params and vars.
        TypeEnv init_env;
        for (auto& param : *(func_node / Params))
        {
          auto key = std::string((param / LocalId)->location().view());
          init_env[key] = resolve_type(param / Type);
        }
        for (auto& var : *(func_node / Vars))
        {
          auto key = std::string(var->location().view());
          init_env[key] = Node(Dyn);
        }

        // Environment fingerprint for convergence detection.
        auto make_fingerprint = [&](const TypeEnv& e) -> std::string {
          std::map<std::string, std::string> sorted;
          for (auto& [k, v] : e)
            sorted[k] = type_name(v);
          std::string fp;
          for (auto& [k, v] : sorted)
            fp += k + ":" + v + ";";
          return fp;
        };

        // Worklist: process labels until type environments stabilize.
        std::vector<bool> in_wl(n_fs_labels, true);
        std::queue<size_t> wl;
        for (size_t i = 0; i < n_fs_labels; i++)
          wl.push(i);

        size_t iterations = 0;
        size_t max_iterations = n_fs_labels * 10;

        while (!wl.empty() && iterations < max_iterations)
        {
          iterations++;
          auto idx = wl.front();
          wl.pop();
          in_wl[idx] = false;

          if (!label_vec[idx])
            continue;

          // Merge entry env from predecessors.
          env.clear();
          if (idx == start_idx)
            env = init_env;

          auto& ls = func_state.labels.at(idx);
          for (auto pred_idx : ls.pred)
          {
            auto this_name = std::string(
              (label_vec[idx] / LabelId)->location().view());
            auto& be = branch_exits[pred_idx];
            auto it = be.find(this_name);
            TypeEnv* pred_exit =
              (it != be.end()) ? &it->second : &exit_envs[pred_idx];

            for (auto& [key, type] : *pred_exit)
            {
              auto eit = env.find(key);
              if (eit == env.end())
                env[key] = clone(type);
              else
                eit->second = type_merge(eit->second, type);
            }
          }

          // Process body instructions.
          auto body = label_vec[idx] / Body;
          for (auto& inst : *body)
            process_node(inst);

          // Process terminator.
          auto term = label_vec[idx]->back();
          process_node(term);

          // For Cond preceded by Typetest, create branch-specific exit envs
          // that narrow the source variable's type on each branch.
          branch_exits[idx].clear();
          if (term == Cond)
          {
            auto cond_var = term / LocalId;

            // Check if the last body statement is a Typetest whose dst
            // matches the Cond operand.
            if (body->size() > 0)
            {
              auto last_stmt = body->back();
              if (
                last_stmt == Typetest &&
                (last_stmt / LocalId)->location() == cond_var->location())
              {
                auto src_name =
                  std::string((last_stmt / Rhs)->location().view());
                auto src_type =
                  resolve_type(get_type(env, last_stmt / Rhs));
                auto tested_type = resolve_type(last_stmt / Type);

                auto true_name =
                  std::string((term / Lhs)->location().view());
                auto false_name =
                  std::string((term / Rhs)->location().view());

                // True branch: src narrowed TO the tested type.
                TypeEnv true_env = env;
                true_env[src_name] = clone(tested_type);
                branch_exits[idx][true_name] = std::move(true_env);

                // False branch: src narrowed to type MINUS the tested type.
                TypeEnv false_env = env;
                if (src_type)
                  false_env[src_name] =
                    type_subtract(src_type, tested_type);
                branch_exits[idx][false_name] = std::move(false_env);
              }
            }
          }

          // Check if exit env changed; if so, reprocess successors.
          auto fp = make_fingerprint(env);
          if (fp != fingerprints[idx])
          {
            fingerprints[idx] = fp;
            exit_envs[idx] = env;

            for (auto succ_idx : ls.succ)
            {
              if (!in_wl[succ_idx])
              {
                in_wl[succ_idx] = true;
                wl.push(succ_idx);
              }
            }
          }
        }

        // Final error-checking pass with converged type environments.
        checking = true;
        for (size_t idx = 0; idx < n_fs_labels; idx++)
        {
          if (!label_vec[idx])
            continue;

          // Recompute entry env from converged exit envs.
          env.clear();
          if (idx == start_idx)
            env = init_env;

          auto& ls2 = func_state.labels.at(idx);
          for (auto pred_idx : ls2.pred)
          {
            auto this_name = std::string(
              (label_vec[idx] / LabelId)->location().view());
            auto& be = branch_exits[pred_idx];
            auto it = be.find(this_name);
            TypeEnv* pred_exit =
              (it != be.end()) ? &it->second : &exit_envs[pred_idx];

            for (auto& [key, type] : *pred_exit)
            {
              auto eit = env.find(key);
              if (eit == env.end())
                env[key] = clone(type);
              else
                eit->second = type_merge(eit->second, type);
            }
          }

          // Process body instructions and terminator for errors.
          auto body2 = label_vec[idx] / Body;
          for (auto& inst : *body2)
            process_node(inst);

          auto term2 = label_vec[idx]->back();
          process_node(term2);
        }
        checking = false;
      }

      for (auto& [n, msg] : errors)
        n->parent()->replace(n, err(clone(n), msg));

      return 0;
    });

    return p;
  }
}
