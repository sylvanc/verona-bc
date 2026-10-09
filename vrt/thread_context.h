#pragma once

#include "location.h"
#include "stack.h"
#include "thread.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include <vrt/error.h>
#include <vrt/value.h>

namespace vrt
{
  struct ErrorBoundary;
  struct Header;

  struct RaisedValue
  {
    uintptr_t type_id;
    std::vector<std::byte> storage;
  };

  /** Language-raise target associated with one active logical frame. */
  struct Continuation
  {
    Continuation* parent = nullptr;
    Frame* frame = nullptr;
    std::jmp_buf state{};
    std::optional<RaisedValue> raised_value{};
  };

  /** Runtime state bound to one native thread and native call stack. */
  struct ThreadContext
  {
    Thread thread{};
    Stack stack{};
    Continuation* continuation = nullptr;
    ErrorBoundary* error_boundary = nullptr;

    /** Return the context bound to the calling native thread. */
    static ThreadContext& get();

    /** Return the bound context, or null if it is uninitialized. */
    static ThreadContext* try_get();

    /** Bind a fresh context to the calling native thread. */
    static void init();

    /** Destroy the context bound to the calling native thread. */
    static void deinit();

    /** Relocate a current-frame allocation so it survives a normal return. */
    void escape(Header* header);

    /** Relocate or validate an allocation so it survives through target. */
    void escape_to(Header* header, Frame* target);

    /** Raise an encoded value through an older active stack Location. */
    [[noreturn]] void
    raise(uintptr_t type_id, const void* value_storage, Location target);

    /** Raise a runtime Error to the innermost error boundary. */
    [[noreturn]] void raise_error(Error error);

    /** Invoke function under a nested runtime Error boundary. */
    [[nodiscard]] ErrorInfo
    try_invoke(InvocationFunction function, void* user_context);

    /** Run cleanup while preserving any caller frame and contain its Error. */
    [[nodiscard]] ErrorInfo
    run_cleanup(InvocationFunction function, void* user_context);

    /** Destroy frames through, but not including, target. */
    void unwind_frames(Frame* target);
  };
}
