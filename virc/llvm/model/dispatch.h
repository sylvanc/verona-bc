#pragma once

#include "representation.h"

#include <cstddef>
#include <vector>

namespace llvm
{
  class GlobalVariable;
}

namespace virc
{
  namespace llvm_backend
  {
    struct LookupTarget
    {
      std::size_t class_id;
      llvm::GlobalVariable* descriptor;
    };

    struct LookupPlan
    {
      LoweredSignature signature;
      std::vector<LookupTarget> targets;
    };
  }
}