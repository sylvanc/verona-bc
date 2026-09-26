#pragma once

#include "bytecode.h"
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
  PassDef assignids(std::shared_ptr<Bytecode> state);
  PassDef validids(std::shared_ptr<Bytecode> state);
  PassDef liveness(std::shared_ptr<Bytecode> state);
  PassDef typecheck(std::shared_ptr<Bytecode> state);
  PassDef optimize(std::shared_ptr<Bytecode> state);

}
