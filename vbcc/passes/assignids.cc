#include "../irsubtype.h"
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
            // Duplicate name. Check type compatibility.
            auto existing = state->get_symbol(_(Symbol) / SymbolId);
            assert(existing);
            auto top = _(Symbol)->scope();
            assert(top);

            auto ep = existing / FFIParams;
            auto np = _(Symbol) / FFIParams;
            auto er = existing / Return;
            auto nr = _(Symbol) / Return;

            bool compatible = ((existing / Lhs)->location().view() ==
                               (_(Symbol) / Lhs)->location().view()) &&
              ((existing / Rhs)->location().view() ==
               (_(Symbol) / Rhs)->location().view()) &&
              IRSubtype.invariant(top, er, nr) &&
              std::equal(ep->begin(),
                         ep->end(),
                         np->begin(),
                         np->end(),
                         [&](auto& a, auto& b) {
                           return IRSubtype.invariant(top, a, b);
                         });

            if (!compatible)
            {
              state->error = true;
              return err(
                       _(Symbol) / SymbolId,
                       "incompatible FFI symbol redefinition")
                << errmsg("previous definition here:")
                << errloc(existing / SymbolId);
            }

            return NoChange;
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

          if (!state->add_typealias(_(Type)))
          {
            state->error = true;
            return err(_(Type) / TypeId, "duplicate type alias name");
          }

          return NoChange;
        },

        // Accumulate non-complex primitive classes.
        T(Primitive)[Primitive]
            << (T(None,
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
                  Ptr)[Type] *
                T(Methods)) >>
          [state](Match& _) -> Node {
          auto& slot = state->primitives.at(+val(_(Type)));

          if (slot)
          {
            state->error = true;
            return err(_(Type), "duplicate primitive class");
          }

          slot = _(Primitive);
          return NoChange;
        },

        // Accumulate user-defined classes.
        T(Class)[Class] >> [state](Match& _) -> Node {
          auto type_id = state->get_typealias_id(_(Class) / ClassId);

          if (type_id)
          {
            state->error = true;
            return err(_(Type) / TypeId, "class shadows type alias name");
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
            return err(method / FunctionId, "unknown function");
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
            func_state.add_register(var / LocalId);

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
      }};

    p.post([state](auto top) {
      state->top = top;
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

      // Build CFG pred/succ edges from terminators. This must happen in
      // assignids (not liveness) because typecheck needs pred/succ for
      // its flow analysis, and liveness may run after typecheck/optimize.
      for (auto& func_node : *top)
      {
        if (!func_node->type().in({Func, FuncOnce}))
          continue;

        auto& func_state = state->get_func(func_node / FunctionId);

        for (auto& label : *(func_node / Labels))
        {
          auto term = label->back();
          auto pred = func_state.get_label_id(label / LabelId);

          if (!pred)
            continue;

          if (term == Jump)
          {
            auto succ = func_state.get_label_id(term / LabelId);

            if (succ)
            {
              func_state.labels.at(*pred).succ.push_back(*succ);
              func_state.labels.at(*succ).pred.push_back(*pred);
            }
          }
          else if (term == Cond)
          {
            auto lhs = func_state.get_label_id(term / Lhs);
            auto rhs = func_state.get_label_id(term / Rhs);

            if (lhs)
            {
              func_state.labels.at(*pred).succ.push_back(*lhs);
              func_state.labels.at(*lhs).pred.push_back(*pred);
            }

            if (rhs)
            {
              func_state.labels.at(*pred).succ.push_back(*rhs);
              func_state.labels.at(*rhs).pred.push_back(*pred);
            }
          }
        }
      }

      // Reserve types for cown i32 (main), [u8] (arg), [[u8]] (argv),
      // `ref dyn` (unknown RegisterRef types), [usize] (FFI struct offsets),
      // and the fixed FFIStruct result tuple. This has to happen after all
      // classes have been added, but before any complex primitives.
      state->typ(Cown << I32);
      state->typ(Array << U8);
      state->typ(Array << (Array << U8));
      state->typ(Ref << Dyn);
      state->typ(Array << USize);
      state->typ(ffi_struct_result_type());
      return 0;
    });

    return p;
  }
}
