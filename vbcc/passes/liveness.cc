#include "../bitset.h"
#include "../lang.h"

#include <queue>

namespace vbcc
{
  PassDef liveness(std::shared_ptr<Bytecode> state)
  {
    PassDef p{"liveness", wfIR, dir::topdown | dir::once, {}};

    p.post([state](auto top) {
      FuncState* func = nullptr;
      LabelState* label = nullptr;
      Bitset vars;

      auto def = [&](Node& id) {
        auto r = *func->get_register_id(id);
        std::tuple<bool, std::string> def_ret = label->def(r, id, vars.test(r));

        if (!std::get<0>(def_ret))
        {
          state->error = true;
          id->parent()->replace(id, err(clone(id), std::get<1>(def_ret)));
        }
      };

      auto use = [&](Node& id) {
        if (!label->use(*func->get_register_id(id), id))
        {
          state->error = true;
          id->parent()->replace(id, err(clone(id), "use of dead register"));
        }
      };

      auto kill = [&](Node& id) {
        if (!label->kill(*func->get_register_id(id)))
        {
          state->error = true;
          id->parent()->replace(id, err(clone(id), "use of dead register"));
        }
      };

      top->traverse(
        [&](auto node) {
          if (node == Func)
          {
            func = &state->get_func(node / FunctionId);
            label = nullptr;
            vars.resize(func->register_names.size());

            for (auto var : *(node / Vars))
              vars.set(*func->get_register_id(var));
          }
          else if (node == Label)
          {
            label = &func->get_label(node / LabelId);
          }
          else if (node == Move)
          {
            kill(node / Rhs);
            def(node / LocalId);
          }
          else if (node == Drop)
          {
            kill(node / LocalId);
          }
          else if (node->in({HeapArray, Add, Sub, Mul, Div,     Mod,  Pow, And,
                             Or,        Xor, Shl, Shr, Eq,      Ne,   Lt,  Le,
                             Gt,        Ge,  Min, Max, LogBase, Atan2}))
          {
            use(node / Lhs);
            use(node / Rhs);
            def(node / LocalId);
          }
          else if (node->in({Convert, Copy,   HeapArrayConst, ArrayRef, Load,
                             Store,   Lookup, Typetest,       Neg,      Not,
                             Abs,     Ceil,   Floor,          Exp,      Log,
                             Sqrt,    Cbrt,   IsInf,          IsNaN,    Sin,
                             Cos,     Tan,    Asin,           Acos,     Atan,
                             Sinh,    Cosh,   Tanh,           Asinh,    Acosh,
                             Atanh,   Len,    MakePtr,        Read}))
          {
            use(node / Rhs);
            def(node / LocalId);
          }
          else if (node->in(
                     {Const,
                      ConstStr,
                      NewArray,
                      NewArrayConst,
                      StackArray,
                      StackArrayConst,
                      RegionArray,
                      RegionArrayConst,
                      RegisterRef,
                      FieldRef,
                      ArrayRefConst,
                      FnPointer,
                      FFI,
                      Const_E,
                      Const_Pi,
                      Const_Inf,
                      Const_NaN}))
          {
            def(node / LocalId);
          }
          else if (node->in({Return, Raise, Throw, TailcallDyn}))
          {
            kill(node / LocalId);
          }
          else if (node == Arg)
          {
            if (node / Type == ArgCopy)
              use(node / Rhs);
            else
              kill(node / Rhs);
          }
          else if (node == MoveArg)
          {
            kill(node / Rhs);
          }
          else if (node == Jump)
          {
            auto& func_state = state->get_func(node->parent(Func) / FunctionId);
            auto pred = *func_state.get_label_id(node->parent(Label) / LabelId);
            auto succ = *func_state.get_label_id(node / LabelId);
            func_state.labels.at(pred).succ.push_back(succ);
            func_state.labels.at(succ).pred.push_back(pred);
          }
          else if (node == Cond)
          {
            use(node / LocalId);
            auto& func_state = state->get_func(node->parent(Func) / FunctionId);
            auto pred = *func_state.get_label_id(node->parent(Label) / LabelId);
            auto lhs = *func_state.get_label_id(node / Lhs);
            auto rhs = *func_state.get_label_id(node / Rhs);
            func_state.labels.at(pred).succ.push_back(lhs);
            func_state.labels.at(pred).succ.push_back(rhs);
            func_state.labels.at(lhs).pred.push_back(pred);
            func_state.labels.at(rhs).pred.push_back(pred);
          }
          else if (node == Error)
          {
            return false;
          }

          return true;
        },
        [&](auto node) {
          // Handle these in post, because the arguments will be pushed first.
          if (node->in({Heap, CallDyn, SubcallDyn, TryDyn, WhenDyn}))
          {
            use(node / Rhs);
            def(node / LocalId);
          }
          else if (node->in({New, Stack, Region, Call, Subcall, Try, When}))
          {
            def(node / LocalId);
          }
        });

      top->traverse([&](auto node) {
        if (node == Func)
        {
          auto target = (node / Labels)->front() / LabelId;
          auto& func_state = state->get_func(node / FunctionId);
          auto vars = Bitset(func_state.register_names.size());

          for (auto var : *(node / Vars))
            vars.set(*func_state.get_register_id(var));

          // Backward data-flow.
          std::queue<size_t> wl;
          for (size_t i = 0; i < func_state.labels.size(); i++)
            wl.push(i);

          while (!wl.empty())
          {
            auto id = wl.front();
            wl.pop();

            // Calculate a new out-set that is everything our successors need.
            // Calculate a new defd set that is everything our successors
            // define.
            auto& l = func_state.labels.at(id);
            auto new_out = l.out;
            auto new_defd = Bitset(func_state.register_names.size());

            for (auto succ_id : l.succ)
            {
              auto& succ = func_state.labels.at(succ_id);
              new_out |= succ.in;
              new_defd |= succ.defd;

              auto redef = l.defd & succ.defd & ~vars;

              for (size_t r = 0; r < func_state.register_names.size(); r++)
              {
                if (succ.defd.test(r) && !l.first_def.at(r))
                  l.first_def[r] = succ.first_def.at(r);

                if (succ.first_use[r])
                  l.first_use[r] = succ.first_use.at(r);
              }

              if (redef)
              {
                for (size_t r = 0; r < func_state.register_names.size(); r++)
                {
                  if (
                    redef.test(r) &&
                    (l.first_def.at(r) != succ.first_def.at(r)))
                  {
                    state->error = true;
                    node << err(
                      clone(succ.first_def.at(r)),
                      "label redefines non-variable register");
                    return true;
                  }
                }
              }
            }

            if (new_out & l.dead)
            {
              for (auto succ_id : l.succ)
              {
                auto& succ = func_state.labels.at(succ_id);

                for (size_t r = 0; r < func_state.register_names.size(); r++)
                {
                  if (succ.in.test(r) && l.dead.test(r))
                  {
                    state->error = true;
                    node << err(
                      clone(succ.first_use.at(r)),
                      "label requires dead register");
                    return true;
                  }
                }
              }
            }

            // Keep new_defd.
            l.defd |= new_defd;

            // Calculate a new in-set that is our out-set, minus our own
            // out-set, plus our own in-set.
            auto new_in = new_out;
            new_in &= ~l.out;
            new_in |= l.in;

            // If the new in-set is different from the old one, keep both the
            // new in-set and the new out-set, and add all predecessors to the
            // worklist.
            if (new_in != l.in)
            {
              l.in = new_in;
              l.out = new_out;

              for (auto pred_id : l.pred)
                wl.push(pred_id);
            }
          }

          // Check that everything is defined.
          auto& label = func_state.get_label(target);
          auto params = Bitset(func_state.register_names.size());

          for (auto param : *(node / Params))
            params.set(*func_state.get_register_id(param / LocalId));

          // undef_registers is the set of registers that are still required
          // that are not paramters
          auto undef_registers = label.in & ~params;
          if (undef_registers)
          {
            for (size_t r = 0; r < func_state.register_names.size(); r++)
            {
              if (undef_registers.test(r))
              {
                state->error = true;
                node << err(
                  clone(label.first_use.at(r)), "use of undefined register");
                return true;
              }
            }
          }

          // Check for multiple assignment to non-variable params.
          auto bad_assign = params & label.defd & ~vars;

          if (bad_assign)
          {
            for (size_t r = 0; r < func_state.register_names.size(); r++)
            {
              if (bad_assign.test(r))
              {
                state->error = true;
                node << err(
                  clone(label.first_use.at(r)),
                  "label writes to non-variable parameter");
                return true;
              }
            }
          }

          for (auto& l : func_state.labels)
          {
            auto unneeded = Bitset(func_state.register_names.size());

            for (auto succ_idx : l.succ)
              unneeded |= func_state.labels.at(succ_idx).in;

            unneeded = l.out & ~unneeded;

            for (size_t r = 0; r < func_state.register_names.size(); r++)
            {
              if (unneeded.test(r))
                l.automove(r);
            }
          }
        }
        else if (node == Error)
        {
          return false;
        }

        return true;
      });

      return 0;
    });

    return p;
  }
}
