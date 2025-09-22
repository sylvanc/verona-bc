#include "../lang.h"

namespace vbcc
{
  PassDef validids(std::shared_ptr<Bytecode> state)
  {
    return {
      "validids",
      wfIR,
      dir::bottomup | dir::once,
      {
        // Accumulate complex primitive classes. This happens in this pass to
        // be able to call state->typ(), which depends on all user-defined
        // classes already having been assigned an id.
        T(Primitive)[Primitive] << (T(Array, Ref, Cown)[Type] * T(Methods)) >>
          [state](Match& _) -> Node {
          auto primitive = _(Primitive);
          auto type = _(Type);
          auto type_id = state->typ(type);
          auto idx = type_id - (state->classes.size() + NumPrimitiveClasses);

          if (state->complex_primitives.size() <= idx)
            state->complex_primitives.resize(idx + 1);

          auto& slot = state->complex_primitives.at(idx);

          if (slot)
          {
            state->error = true;
            return err(type, "duplicate primitive class");
          }

          slot = primitive;
          return NoChange;
        },

        T(TypeId)[TypeId] >> [state](Match& _) -> Node {
          auto id = state->get_typealias_id(_(TypeId));

          if (!id)
          {
            state->error = true;
            return err(_(TypeId), "unknown type alias");
          }

          return NoChange;
        },

        T(ClassId)[ClassId] >> [state](Match& _) -> Node {
          auto type_id = state->get_typealias_id(_(ClassId));

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
          auto symbol = state->symbols.at(*id);
          auto params = symbol / FFIParams;
          auto varargs = (symbol / Vararg) == Vararg;

          if (
            (args->size() < params->size()) ||
            (!varargs && (args->size() > params->size())))
          {
            state->error = true;
            return err(args, "wrong number of arguments");
          }

          return NoChange;
        },

        T(New, Stack, Heap, Region)[New] >> [state](Match& _) -> Node {
          auto alloc = _(New);
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
  }
}
