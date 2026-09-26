#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace virc
{
  struct VecHash
  {
    size_t operator()(const std::vector<uint8_t>& v) const noexcept;
  };
}
