#pragma once

#include <atomic>
#include <cstdint>

namespace vrt
{
  using RC = uintptr_t;

  /** Atomic view over existing RC storage; does not own separate storage. */
  using ARC = std::atomic_ref<RC>;
}