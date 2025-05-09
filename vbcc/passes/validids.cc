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
        T(TypeId)[TypeId] >> [state](Match& _) -> Node {
          auto id = state->get_type_id(_(TypeId));

          if (!id)
          {
            state->error = true;
            return err(_(TypeId), "unknown type");
          }

          return NoChange;
        },

        T(ClassId)[ClassId] >> [state](Match& _) -> Node {
          auto type_id = state->get_type_id(_(ClassId));

          if (type_id)
            return TypeId ^ _(ClassId);

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
            if (_(FunctionId)->parent() == FnPointer)
            {
              // Can be a SymbolId instead.
              if (state->get_symbol_id(_(FunctionId)))
                return SymbolId ^ _(FunctionId);
            }

            state->error = true;
            return err(_(FunctionId), "unknown function");
          }

          return NoChange;
        },

        T(SymbolId)[SymbolId] >> [state](Match& _) -> Node {
          auto id = state->get_symbol_id(_(SymbolId));

          if (!id)
          {
            state->error = true;
            return err(_(SymbolId), "unknown symbol");
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

        T(Call, Subcall, Try)[Call] >> [state](Match& _) -> Node {
          auto call = _(Call);
          auto& func_state = state->get_func(call / FunctionId);

          if ((call / Args)->size() != func_state.params)
          {
            state->error = true;
            return err(call / Args, "wrong number of arguments");
          }

          return NoChange;
        },

        T(FFI)[FFI] >> [state](Match& _) -> Node {
          auto ffi = _(FFI);
          auto id = state->get_symbol_id(ffi / SymbolId);

          if (!id)
            return NoChange;

          auto args = ffi / Args;
          auto params = state->symbols.at(*id) / FFIParams;

          if (args->size() != params->size())
          {
            state->error = true;
            return err(args, "wrong number of arguments");
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
