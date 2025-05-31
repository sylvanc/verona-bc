#include "../lang.h"

namespace vc
{
  PassDef anf()
  {
    PassDef p{
      "anf",
      wfPassANF,
      dir::topdown,
      {
        //
      }};

    return p;
  }
}
