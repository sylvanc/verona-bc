#pragma once

#include "model/compilation.h"
#include "passes/patterns.h"
#include "support/diagnostics.h"
#include "support/literals.h"

#include <vir.h>

namespace virc
{
  using namespace trieste;
  using namespace vir;
  using vir::Source;

  Parse parser();
  PassDef statements();
  PassDef labels();
  PassDef memo();
  PassDef assign_ids(std::shared_ptr<Compilation> state);
  PassDef validate_ids(std::shared_ptr<Compilation> state);
  PassDef liveness(std::shared_ptr<Compilation> state);
  PassDef typecheck(std::shared_ptr<Compilation> state);
  PassDef optimize(std::shared_ptr<Compilation> state);

}
