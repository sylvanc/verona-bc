#pragma once

#include <cstddef>
#include <cstdint>

namespace virc
{
  inline constexpr auto MainFunctionId = size_t(0);
  inline constexpr auto FinalizerMethodId = size_t(0);
  inline constexpr auto CallbackMethodId = size_t(1);
  inline constexpr auto DynamicTypeId = uint32_t(-1);
}
