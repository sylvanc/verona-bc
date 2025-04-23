#pragma once

#include "types.h"

#include <cstddef>
#include <vector>

namespace vbci
{
  struct Function
  {
    std::vector<PC> labels;
    std::vector<Type> params;
    Local registers;
    Type return_type;
    size_t debug_info;
  };
}
