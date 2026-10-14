#pragma once

#include <cstddef>
#include <cstdint>

namespace virc
{
  // Backend-neutral semantic IDs owned by VIRC's compilation model. Emitters
  // map them to output contracts without making the compilation model
  // VBC-specific.
  inline constexpr auto MainFunctionId = size_t(0);
  inline constexpr auto FinalizerMethodId = size_t(0);
  inline constexpr auto CallbackMethodId = size_t(1);
  inline constexpr auto DynamicTypeId = uint32_t(-1);
}