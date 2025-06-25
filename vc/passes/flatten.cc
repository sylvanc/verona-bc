#include "../lang.h"

namespace vc
{
  bool is_std_builtin(Node classdef)
  {
    assert(classdef == ClassDef);
    auto p = classdef->parent(ClassDef);

    if (!p || ((p / Ident)->location().view() != "builtin"))
      return false;

    p = p->parent(ClassDef);
    return (p && (p / Ident)->location().view() == "std") &&
      (p->parent() == Top);
  }

  Node primitive_type(Node classdef)
  {
    if (!is_std_builtin(classdef))
      return {};

    auto name = (classdef / Ident)->location().view();

    if (name == "none")
      return None;
    else if (name == "bool")
      return Bool;
    else if (name == "i8")
      return I8;
    else if (name == "i16")
      return I16;
    else if (name == "i32")
      return I32;
    else if (name == "i64")
      return I64;
    else if (name == "u8")
      return U8;
    else if (name == "u16")
      return U16;
    else if (name == "u32")
      return U32;
    else if (name == "u64")
      return U64;
    else if (name == "ilong")
      return ILong;
    else if (name == "ulong")
      return ULong;
    else if (name == "isize")
      return ISize;
    else if (name == "usize")
      return USize;
    else if (name == "f32")
      return F32;
    else if (name == "f64")
      return F64;

    return {};
  }

  Node make_path(Token t, Node from, Node id)
  {
    // TODO: type arguments
    auto name = std::string(id->location().view());
    auto p = from->parent(ClassDef);

    while (p)
    {
      name = std::format("{}::{}", (p / Ident)->location().view(), name);
      p = p->parent(ClassDef);
    }

    return t ^ name;
  }

  Node make_classid(Node classdef)
  {
    // TODO: type arguments
    assert(classdef == ClassDef);
    return make_path(ClassId, classdef, classdef / Ident);
  }

  Node make_methodid(Node ref, Node ident, Node typeargs, Node args)
  {
    // TODO: type arguments
    (void)typeargs;
    auto name = std::format(
      "{}.{}{}",
      ident->location().view(),
      args->location().view(),
      (ref == Lhs) ? ".ref" : "");
    return MethodId ^ name;
  }

  Node make_methodid(Node function)
  {
    // TODO: type arguments
    assert(function == Function);
    auto name = std::format(
      "{}.{}{}",
      (function / Ident)->location().view(),
      (function / Params)->size(),
      (function / Lhs) == Lhs ? ".ref" : "");
    return MethodId ^ name;
  }

  Node make_functionid(Node function)
  {
    if (function == Error)
      return function;

    assert(function == Function);
    return make_path(FunctionId, function, make_methodid(function));
  }

  Node make_irtype(Node type)
  {
    if (type == Type)
    {
      if (type->empty())
        return Dyn;

      type = type->front();
    }

    if (type == RefType)
    {
      return Ref << make_irtype(type->front());
    }
    else if (type == TypeName)
    {
      auto def = resolve(type);

      if (def == Error)
      {
        return def;
      }
      else if (def == ClassDef)
      {
        auto prim = primitive_type(def);
        return prim ? prim : make_classid(def);
      }
      else if (def == TypeAlias)
      {
        return make_irtype(def / Type);
      }
      else if (def == TypeParam)
      {
        return Dyn;
      }
    }

    // TODO: IR is primitive, classid, typeid, array, cown, union.
    // lang is union, isect, tuple, func.
    return Dyn;
  }

  PassDef flatten()
  {
    PassDef p{
      "flatten",
      wfIR,
      dir::bottomup,
      {
        T(ClassDef)[ClassDef] >>
          [](Match& _) {
            auto def = _(ClassDef);
            auto prim = primitive_type(def);
            Node fields = Fields;
            Node methods = Methods;
            Node seq = Seq;

            if (prim)
              seq << (Primitive << prim << methods);
            else
              seq << (Class << make_classid(def) << fields << methods);

            for (auto& child : *(def / ClassBody))
            {
              if (child == FieldDef)
              {
                if (prim)
                  return err(_(ClassDef), "A primitive type can't have fields");

                fields
                  << (Field << (FieldId ^ (child / Ident))
                            << make_irtype(child / Type));
              }
              else if (child == Function)
              {
                // TODO: type parameters
                auto functionid = make_functionid(child);
                methods << (Method << make_methodid(child) << functionid);
                Node params = Params;

                for (auto& param : *(child / Params))
                {
                  params
                    << (Param << (LocalId ^ (param / Ident))
                              << make_irtype(param / Type));
                }

                seq
                  << (Func << clone(functionid) << params
                           << make_irtype(child / Type) << (child / Labels));
              }
              else if (child->in({Primitive, Class, Func}))
              {
                seq << child;
              }
            }

            return seq;
          },

        T(New) << (T(LocalId)[LocalId] * T(Type)[Type] * T(Args)[Args]) >>
          [](Match& _) {
            return New << _(LocalId) << make_irtype(_(Type)) << _(Args);
          },

        T(NewArrayConst)
            << (T(LocalId)[LocalId] * T(Type)[Type] * T(Int)[Int]) >>
          [](Match& _) {
            return NewArrayConst << _(LocalId) << make_irtype(_(Type))
                                 << _(Int);
          },

        T(Typetest) << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Type)[Type]) >>
          [](Match& _) {
            return Typetest << _(Lhs) << _(Rhs) << make_irtype(_(Type));
          },

        T(Lookup)
            << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Lhs, Rhs)[Ref] *
                T(Ident, SymbolId)[Ident] * T(TypeArgs)[TypeArgs] *
                T(Int)[Args]) >>
          [](Match& _) {
            return Lookup << _(Lhs) << _(Rhs)
                          << make_methodid(
                               _(Ref), _(Ident), _(TypeArgs), _(Args));
          },

        T(Call)
            << (T(LocalId)[LocalId] * T(FnPointer)[FnPointer] *
                T(Args)[Args]) >>
          [](Match& _) {
            auto fp = _(FnPointer);
            auto def = resolve_qname(fp / QName, fp / Lhs, _(Args)->size());
            return Call << _(LocalId) << make_functionid(def) << _(Args);
          },
      }};

    p.post([](auto top) {
      Nodes to_remove;
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node->in({Use, TypeAlias}))
        {
          to_remove.push_back(node);
        }

        return ok;
      });

      for (auto& node : to_remove)
        node->parent()->replace(node);

      return 0;
    });

    return p;
  };
}
