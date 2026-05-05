#pragma once

#include "ident.h"

#include <cstddef>
#include <unordered_set>
#include <vector>

namespace vbci
{
  struct Function
  {
    std::vector<PC> labels;
    std::vector<uint32_t> param_types;
    // Set of register ids that are Var slots (vs. SSA temporaries).
    // Vars are NOT contiguous in register-id space — optimize-inlining
    // can add a Var whose register id falls outside the simple
    // [params, params+vars) range.
    //
    // The runtime only needs to know WHICH registers are vars (so that
    // RegisterRef can target them); declared types live in vbcc's
    // static type checker. vbci is dynamic — values carry their own
    // types and per-slot type checks at runtime would just duplicate
    // vbcc's static checks.
    std::unordered_set<size_t> var_registers;
    size_t registers;
    uint32_t return_type;
    size_t debug_info;
  };
}
