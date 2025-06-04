#pragma once

#include "types.h"

#include <cstddef>
#include <vector>

namespace vbci
{
  struct Function
  {
    std::vector<PC> labels;
    std::vector<TypeId> param_types;
    size_t registers;
    TypeId return_type;
    size_t debug_info;
  };
}
