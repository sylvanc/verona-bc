#include "../lang.h"

namespace vbcc
{
  PassDef assignids(std::shared_ptr<State> state)
  {
    PassDef p{
      "assignids",
      wfIR,
      dir::topdown | dir::once,
      {
        // Accumulate primitive classes.
        T(Primitive)[Primitive] >> [state](Match& _) -> Node {
          auto primitive = _(Primitive);
          auto vtype = val(primitive / Type);

          if (!vtype)
          {
            state->error = true;
            return err(primitive / Type, "unknown primitive type");
          }

          auto& slot = state->primitives.at(*vtype);

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
          if (!state->add_class(_(Class)))
          {
            state->error = true;
            return err(_(Class) / ClassId, "duplicate class name");
          }

          if ((_(Class) / Fields)->size() > MaxRegisters)
          {
            state->error = true;
            return err(_(Class) / ClassId, "class has too many fields");
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
            return err(func / FunctionId, "duplicate function name");
          }

          auto& func_state = state->add_func(func);

          if ((func / Labels)->size() == 0)
          {
            state->error = true;
            return err(func_id, "function has no labels");
          }

          if ((func / Labels)->size() >= MaxRegisters)
          {
            state->error = true;
            return err(func_id, "function has too many labels");
          }

          if ((func / Params)->size() >= MaxRegisters)
          {
            state->error = true;
            return err(func_id, "function has too many params");
          }

          // Register label names.
          for (auto label : *(func / Labels))
          {
            if (!func_state.add_label(label / LabelId))
            {
              state->error = true;
              return err(label / LabelId, "duplicate label name");
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

        // Define all destination registers.
        Def[Body] >> [state](Match& _) -> Node {
          auto stmt = _(Body);
          auto dst = stmt / LocalId;
          auto& func_state = state->get_func(dst->parent(Func) / FunctionId);

          if (func_state.add_register(dst))
          {
            // Check that no register used in this statement was just defined.
            for (auto& child : *stmt)
            {
              if ((&*dst != &*child) && dst->equals(child))
              {
                state->error = true;
                return err(child, "register used before defined");
              }
            }
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
      }};

    p.post([state](auto top) {
      if (!state->functions.at(0).func)
      {
        state->error = true;
        top << err(Func, "missing main function");
      }

      return 0;
    });

    return p;
  }
}
