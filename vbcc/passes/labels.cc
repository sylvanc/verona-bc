#include "../lang.h"

namespace vbcc
{
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
