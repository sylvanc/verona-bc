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
    Type return_type;
  };
}
