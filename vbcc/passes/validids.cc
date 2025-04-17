#include "../lang.h"

namespace vbcc
{
  PassDef validids(std::shared_ptr<State> state)
  {
    return {
      "validids",
      wfPassLabels,
      dir::topdown | dir::once,
      {
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

          return NoChange;
        },

        T(GlobalId)[GlobalId] >> [state](Match& _) -> Node {
          auto id = state->get_func_id(_(GlobalId));

          if (!id)
          {
            state->error = true;
            return err(_(GlobalId), "unknown global");
          }

          return NoChange;
        },
      }};
  }
}
