#pragma once

#include "thread.h"

#include <csetjmp>
#include <cstdint>
#include <optional>

namespace vrt
{
  /** Native control state associated with one active logical frame. */
  struct NativeContinuation
  {
    NativeContinuation* parent = nullptr;
    Frame* frame = nullptr;
    std::jmp_buf state{};
    std::optional<uint64_t> raised_value{};
  };

  /** Native control-transfer state associated with one logical Thread. */
  struct NativeContext
  {
    Thread thread{};
    NativeContinuation* continuation = nullptr;

    /** Return the native context bound to the calling native thread. */
    static NativeContext& get();

    /** Return the bound native context, or null if it is uninitialized. */
    static NativeContext* try_get();

    /** Bind a fresh native context to the calling native thread. */
    static void init();

    /** Destroy the native context bound to the calling native thread. */
    static void deinit();
  };

  /** Destroy frames through, but not including, target. */
  void unwind_frames(NativeContext& context, Frame* target);
}
