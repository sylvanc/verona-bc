#include "../lang.h"

namespace vbcc
{
  const auto Statement = Def / T(Drop, Arg, Source, Offset);
  const auto Terminator =
    T(Tailcall, TailcallDyn, Return, Raise, Throw, Cond, Jump);

  PassDef labels()
  {
    return {
      "VIR",
      wfIR,
      dir::bottomup,
      {
        // Function.
        T(Func)[Func] * T(Label)[Label] >>
          [](Match& _) {
            (_(Func) / Labels) << _(Label);
            return _(Func);
          },

        // Label.
        T(LabelId)[LabelId] * Statement++[Lhs] * Terminator[Rhs] >>
          [](Match& _) {
            return Label << _(LabelId) << (Body << _[Lhs]) << _(Rhs);
          },
      }};
  }
}
