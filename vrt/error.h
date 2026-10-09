#pragma once

#include <csetjmp>
#include <vrt/error.h>

namespace vrt
{
  struct Continuation;
  struct Frame;

  /** Logical-frame state that must survive a nested runtime cleanup. */
  struct FrameBoundary
  {
    Frame* frame;
    Continuation* continuation;
  };

  /** Nested runtime-Error recovery scope, separate from language raises. */
  struct ErrorBoundary
  {
    ErrorBoundary* parent;
    FrameBoundary frame_boundary;
    std::jmp_buf recovery;
    ErrorInfo error{};
  };

  [[noreturn]] void raise_error(Error error);
  [[nodiscard]] ErrorInfo
  try_invoke(InvocationFunction function, void* user_context);
}
