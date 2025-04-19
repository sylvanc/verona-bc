#include "../lang.h"

namespace vbcc
{
  PassDef validids(std::shared_ptr<State> state)
  {
    return {
      "validids",
      wfIR,
      dir::bottomup | dir::once,
      {
        T(Func)[Func] >> [state](Match& _) -> Node {
          auto func = _(Func);
          auto func_id = func / FunctionId;
          auto& func_state = state->get_func(func_id);

          if (func_state.register_idxs.size() >= MaxRegisters)
          {
            state->error = true;
            return err(func_id, "function has too many registers");
          }

          return NoChange;
        },

        T(ClassId)[ClassId] >> [state](Match& _) -> Node {
          auto id = state->get_class_id(_(ClassId));

          if (!id)
          {
            state->error = true;
            return err(_(ClassId), "unknown class");
          }

          return NoChange;
        },

        T(FieldId)[FieldId] >> [state](Match& _) -> Node {
          auto id = state->get_field_id(_(FieldId));

          if (!id)
          {
            state->error = true;
            return err(_(FieldId), "unknown field");
          }

          return NoChange;
        },

        T(MethodId)[MethodId] >> [state](Match& _) -> Node {
          auto id = state->get_method_id(_(MethodId));

          if (!id)
          {
            state->error = true;
            return err(_(MethodId), "unknown method");
          }

          return NoChange;
        },

        T(FunctionId)[FunctionId] >> [state](Match& _) -> Node {
          auto id = state->get_func_id(_(FunctionId));

          if (!id)
          {
            state->error = true;
            return err(_(FunctionId), "unknown function");
          }

          if (*id == MainFuncId)
          {
            if (state->functions.at(MainFuncId).params != 0)
            {
              state->error = true;
              return err(
                _(FunctionId), "main function must take no parameters");
            }
          }

          return NoChange;
        },

        T(GlobalId)[GlobalId] >> [state](Match& /*_*/) -> Node {
          // auto id = state->get_global_id(_(GlobalId));

          // if (!id)
          // {
          //   state->error = true;
          //   return err(_(GlobalId), "unknown global");
          // }

          return NoChange;
        },

        T(Method)[Method] >> [state](Match& _) -> Node {
          auto method = _(Method);
          auto id = state->get_method_id(method / MethodId);

          if (*id == FinalMethodId)
          {
            auto func_id = state->get_func_id(method / FunctionId);
            if (state->functions.at(*func_id).params != 1)
            {
              state->error = true;
              return err(
                method / FunctionId, "finalizer must have one parameter");
            }
          }

          return NoChange;
        },

        T(Stack, Heap, Region)[Stack] >> [state](Match& _) -> Node {
          auto alloc = _(Stack);
          auto id = state->get_class_id(alloc / ClassId);

          if (!id)
            return NoChange;

          auto args = alloc / Args;
          auto fields = state->classes.at(*id) / Fields;

          if (args->size() != fields->size())
          {
            state->error = true;
            return err(args, "wrong number of arguments");
          }

          return NoChange;
        },
      }};
  }
}
