#include "../lang.h"

namespace vbcc
{
  PassDef assignids(std::shared_ptr<Bytecode> state)
  {
    PassDef p{
      "assignids",
      wfIR,
      dir::topdown | dir::once,
      {
        // Accumulate libraries.
        T(Lib)[Lib] >> [state](Match& _) -> Node {
          state->add_library(_(Lib));
          return NoChange;
        },

        // Accumulate symbols.
        T(Symbol)[Symbol] >> [state](Match& _) -> Node {
          if (!state->add_symbol(_(Symbol)))
          {
            state->error = true;
            return err(_(Symbol), "duplicate symbol");
          }

          return NoChange;
        },

        // Accumulate type defs.
        T(Type)[Type] >> [state](Match& _) -> Node {
          auto class_id = state->get_class_id(_(Type) / TypeId);

          if (class_id)
          {
            state->error = true;
            return err(_(Type) / TypeId, "type shadows class name");
          }

          if (!state->add_type(_(Type)))
          {
            state->error = true;
            return err(_(Type) / TypeId, "duplicate class name");
          }

          return NoChange;
        },

        // Accumulate primitive classes.
        T(Primitive)[Primitive] >> [state](Match& _) -> Node {
          auto primitive = _(Primitive);
          auto vtype = val(primitive / Type);
          auto& slot = state->primitives.at(+vtype);

          if (slot)
          {
            state->error = true;
            return err(primitive / Type, "duplicate primitive class");
          }

          slot = primitive;
          return NoChange;
        },

        // Accumulate user-defined classes.
        T(Class)[Class] >> [state](Match& _) -> Node {
          auto type_id = state->get_type_id(_(Class) / ClassId);

          if (type_id)
          {
            state->error = true;
            return err(_(Type) / TypeId, "class shadows type name");
          }

          if (!state->add_class(_(Class)))
          {
            state->error = true;
            return err(_(Class) / ClassId, "duplicate class name");
          }

          return NoChange;
        },

        // Accumulate field names.
        T(Field)[Field] >> [state](Match& _) -> Node {
          state->add_field(_(Field));
          return NoChange;
        },

        // Accumulate method names.
        T(Method)[Method] >> [state](Match& _) -> Node {
          auto method = _(Method);
          state->add_method(method);

          if (!state->get_func_id(method / FunctionId))
          {
            state->error = true;
            return err(method / Rhs, "unknown function");
          }

          return NoChange;
        },

        // Accumulate functions.
        T(Func)[Func] >> [state](Match& _) -> Node {
          auto func = _(Func);
          auto func_id = func / FunctionId;

          if (state->get_func_id(func_id))
          {
            state->error = true;
            return err(func_id, "duplicate function name");
          }

          auto& func_state = state->add_func(func);

          if ((func / Labels)->size() == 0)
          {
            state->error = true;
            return err(func_id, "function has no labels");
          }

          if (*state->get_func_id(func_id) == MainFuncId)
          {
            if (func_state.params != 0)
            {
              state->error = true;
              return err(func_id, "main function must take no parameters");
            }

            if ((func_state.func / Type) != I32)
            {
              state->error = true;
              return err(func_id, "main function must return i32");
            }
          }

          // Register parameter names.
          for (auto param : *(func / Params))
          {
            if (!func_state.add_register(param / LocalId))
            {
              state->error = true;
              return err(param / LocalId, "duplicate parameter name");
            }
          }

          // Register variable names.
          for (auto var : *(func / Vars))
            func_state.add_register(var);

          // Register label names.
          for (auto label : *(func / Labels))
          {
            if (!func_state.add_label(label / LabelId))
            {
              state->error = true;
              return err(label / LabelId, "duplicate label name");
            }
          }

          return NoChange;
        },

        // Define all destination registers.
        Def[Body] >> [state](Match& _) -> Node {
          auto dst = _(Body) / LocalId;
          state->get_func(dst->parent(Func) / FunctionId).add_register(dst);
          return NoChange;
        },

        // Check that all labels in a function are defined.
        T(LabelId)[LabelId] >> [state](Match& _) -> Node {
          auto label = _(LabelId);
          auto& func_state = state->get_func(label->parent(Func) / FunctionId);

          if (!func_state.get_label_id(label))
          {
            state->error = true;
            return err(label, "undefined label");
          }

          return NoChange;
        },

        // Check that all registers in a function are defined.
        T(LocalId)[LocalId] >> [state](Match& _) -> Node {
          auto id = _(LocalId);
          auto& func_state = state->get_func(id->parent(Func) / FunctionId);

          if (!func_state.get_register_id(id))
          {
            state->error = true;
            return err(id, "undefined register");
          }

          return NoChange;
        },

        // Internalize unescaped string literals.
        T(ConstStr)[ConstStr] >> [state](Match& _) -> Node {
          auto str = unescape((_(ConstStr) / String)->location().view());
          ST::exec().string(str);
          _(ConstStr) / String = String ^ str;
          return NoChange;
        },
      }};

    p.post([state](auto top) {
      if (!state->functions.at(MainFuncId).func)
      {
        state->error = true;
        top << err(Func, "missing main function");
      }

      for (auto& func_state : state->functions)
      {
        for (auto& label : func_state.labels)
          label.resize(func_state.register_names.size());
      }

      return 0;
    });

    return p;
  }
}
