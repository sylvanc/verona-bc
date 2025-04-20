#include "../lang.h"

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
        else if (node->in(
                   {Copy,   Heap,    HeapArrayConst, ArrayRef, Load, Store,
                    Lookup, CallDyn, SubcallDyn,     TryDyn,   Neg,  Not,
                    Abs,    Ceil,    Floor,          Exp,      Log,  Sqrt,
                    Cbrt,   IsInf,   IsNaN,          Sin,      Cos,  Tan,
                    Asin,   Acos,    Atan,           Sinh,     Cosh, Tanh,
                    Asinh,  Acosh,   Atanh}))
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
                    Ref,
                    ArrayRefConst,
                    FnPointer,
                    Call,
                    Subcall,
                    Try,
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
        }
        else if (node == Jump)
        {
          auto target = node / LocalId;
          auto& func_state = state->get_func(node->parent(Func) / FunctionId);
          auto& label = func_state.get_label(node->parent(Label) / LabelId);
          auto& target_label = func_state.get_label(target);

          if ((label.out & target_label.in) != target_label.in)
          {
            state->error = true;
            (node / LocalId) =
              err(clone(target), "branch doesn't define needed registers");
          }
        }
        else if (node == Cond)
        {
          auto on_true = node / Lhs;
          auto on_false = node / Rhs;
          auto& func_state = state->get_func(node->parent(Func) / FunctionId);
          auto& label = func_state.get_label(node->parent(Label) / LabelId);
          auto& true_label = func_state.get_label(on_true);
          auto& false_label = func_state.get_label(on_false);

          if ((label.out & true_label.in) != true_label.in)
          {
            state->error = true;
            (node / Lhs) =
              err(clone(on_true), "branch doesn't define needed registers");
          }

          if ((label.out & false_label.in) != false_label.in)
          {
            state->error = true;
            (node / Rhs) =
              err(clone(on_false), "branch doesn't define needed registers");
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
