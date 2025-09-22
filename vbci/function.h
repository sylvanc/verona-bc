#pragma once

#include "ident.h"

#include <cstddef>
#include <vector>

namespace vbci
{
  struct Function
  {
    std::vector<PC> labels;
    std::vector<uint32_t> param_types;
    size_t registers;
    uint32_t return_type;
    size_t debug_info;
  };
}
