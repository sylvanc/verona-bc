#include "../lang.h"

namespace vc
{
  using FFIInitKey = std::pair<const NodeDef*, std::string>;

  Node none_type()
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "none") << TypeArgs));
  }

  FFIInitKey ffi_init_key(const Node& cls, const Node& lib)
  {
    return {cls.get(), std::string((lib / String)->location().view())};
  }

  Node promote_ffi_init(Node func, const Location& id, Node top)
  {
    auto labels = clone(func / Labels);

    for (auto& label : *labels)
    {
      auto term = label / Return;
      if (term != Return)
        continue;

      auto callback = term / LocalId;
      auto result = LocalId ^ top->fresh(l_local);
      auto body = label / Body;
      body << ((AtTeardown ^ term->location()) << clone(callback))
           << (Const << clone(result) << None << None);
      term->replace(callback, clone(result));
    }

    return Function << Once << (Ident ^ id) << clone(func / TypeParams)
                    << clone(func / Params) << none_type()
                    << clone(func / Where) << labels;
  }

  PassDef ffiinit()
  {
    auto defs = std::make_shared<std::map<FFIInitKey, Node>>();

    return {
      "ffiinit",
      wfPassFFIInit,
      dir::bottomup,
      {
        In(Symbols) * T(Function)[Function] >> [defs](Match& _) -> Node {
          auto func = _(Function);

          if ((func / Ident)->location().view() != "init")
            return NoChange;

          auto lib = func->parent(Lib);
          auto cls = lib->parent(ClassDef);
          auto key = ffi_init_key(cls, lib);
          auto previous = defs->find(key);

          if (previous != defs->end())
          {
            auto lib_name = (lib / String)->location().view();
            return err(
                     func / Ident,
                     std::format(
                       "Conflicting 'init' for library \"{}\"", lib_name))
              << errmsg("Previous declaration resolved here:")
              << errloc(previous->second / Ident);
          }

          defs->emplace(key, func);
          auto id = ffi_init_id(lib);
          auto init = promote_ffi_init(func, id, func->parent(Top));
          return Lift << ClassBody << init;
        },
      }};
  }
}
