#pragma once

#include "model/compilation.h"
#include "passes/patterns.h"
#include "support/diagnostics.h"
#include "support/literals.h"

namespace virc
{
  PassDef memo();
  PassDef assign_ids(std::shared_ptr<Compilation> state);
  PassDef validate_ids(std::shared_ptr<Compilation> state);
  PassDef liveness(std::shared_ptr<Compilation> state);
  PassDef typecheck(std::shared_ptr<Compilation> state);
  PassDef optimize(std::shared_ptr<Compilation> state);
}
