#include "../lang.h"

#include <queue>

namespace vbcc
{
  PassDef liveness(std::shared_ptr<State> state)
  {
    PassDef p{"liveness", wfIR, dir::topdown | dir::once, {}};

    p.post([state](auto top) {
      top->traverse([&](auto node) {
        if (node == Move)
        {
          state->kill(node / Rhs);
          state->def(node / LocalId);
        }
        else if (node == Drop)
        {
          state->kill(node / LocalId);
        }
        else if (node->in({HeapArray, Add, Sub, Mul, Div,     Mod,  Pow, And,
                           Or,        Xor, Shl, Shr, Eq,      Ne,   Lt,  Le,
                           Gt,        Ge,  Min, Max, LogBase, Atan2}))
        {
          state->use(node / Lhs);
          state->use(node / Rhs);
          state->def(node / LocalId);
        }
        else if (node->in({Convert,  Copy,       Heap,   HeapArrayConst,
                           ArrayRef, Load,       Store,  Lookup,
                           CallDyn,  SubcallDyn, TryDyn, Typetest,
                           Neg,      Not,        Abs,    Ceil,
                           Floor,    Exp,        Log,    Sqrt,
                           Cbrt,     IsInf,      IsNaN,  Sin,
                           Cos,      Tan,        Asin,   Acos,
                           Atan,     Sinh,       Cosh,   Tanh,
                           Asinh,    Acosh,      Atanh,  Len,
                           ArrayPtr}))
        {
          state->use(node / Rhs);
          state->def(node / LocalId);
        }
        else if (node->in(
                   {Const,
                    Stack,
                    Region,
                    StackArray,
                    StackArrayConst,
                    RegionArray,
                    RegionArrayConst,
                    FieldRef,
                    ArrayRefConst,
                    FnPointer,
                    Call,
                    CallDyn,
                    Subcall,
                    SubcallDyn,
                    Try,
                    TryDyn,
                    FFI,
                    Const_E,
                    Const_Pi,
                    Const_Inf,
                    Const_NaN}))
        {
          state->def(node / LocalId);
        }
        else if (node->in({Return, Raise, Throw, Cond, TailcallDyn}))
        {
          state->use(node / LocalId);
        }
        else if (node == Arg)
        {
          if (node / Type == ArgCopy)
            state->use(node / Rhs);
          else
            state->kill(node / Rhs);
        }
        else if (node == MoveArg)
        {
          state->kill(node / Rhs);
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
      });

      top->traverse([&](auto node) {
        if (node == Func)
        {
          auto target = (node / Labels)->front() / LabelId;
          auto& func_state = state->get_func(node / FunctionId);
          auto& label = func_state.get_label(target);
          std::bitset<MaxRegisters> params;

          for (auto param : *(node / Params))
            params.set(*func_state.get_register_id(param / LocalId));

          if ((params & label.in) != label.in)
          {
            state->error = true;
            node << err(
              clone(target), "branch doesn't define needed registers");
          }

          // Backward data-flow.
          std::queue<uint8_t> wl;
          for (uint8_t i = 0; i < func_state.labels.size(); i++)
            wl.push(i);

          while (!wl.empty())
          {
            auto id = wl.front();
            wl.pop();

            // Calculate a new out-set that is everything our successors need.
            auto& l = func_state.labels.at(id);
            std::bitset<MaxRegisters> new_out;

            for (auto succ_id : l.succ)
            {
              auto& succ = func_state.labels.at(succ_id);
              new_out |= succ.in;
            }

            // Calculate a new in-set that is our out-set, minus our own
            // out-set, plus our own in-set.
            std::bitset<MaxRegisters> new_in = new_out;
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
