#pragma once

#include "location.h"
#include "thread.h"

#include <csetjmp>
#include <cstdint>
#include <optional>
#include <vrt/error.h>
#include <vrt/value.h>

namespace vrt
{
  struct ErrorCatchPoint;

  /** Native control state associated with one active logical frame. */
  struct Continuation
  {
    Continuation* parent = nullptr;
    Frame* frame = nullptr;
    std::jmp_buf state{};
    std::optional<uint64_t> raised_value{};
  };

  /** Runtime state bound to one native thread and native call stack. */
  struct ThreadContext
  {
    Thread thread{};
    Continuation* continuation = nullptr;
    ErrorCatchPoint* error_catch_point = nullptr;

    /** Return the context bound to the calling native thread. */
    static ThreadContext& get();

    /** Return the bound context, or null if it is uninitialized. */
    static ThreadContext* try_get();

    /** Bind a fresh context to the calling native thread. */
    static void init();

    /** Destroy the context bound to the calling native thread. */
    static void deinit();

    /** Raise a type-erased value through an older active stack Location. */
    [[noreturn]] void
    raise(ValueType value_type, uint64_t value, Location target);

    /** Raise a runtime Error to the innermost invocation catch point. */
    [[noreturn]] void raise_error(Error error);

    /** Invoke function under a nested runtime Error catch point. */
    [[nodiscard]] ErrorInfo
    try_invoke(InvocationFunction function, void* user_context);

    /** Destroy frames through, but not including, target. */
    void unwind_frames(Frame* target);
  };
}
