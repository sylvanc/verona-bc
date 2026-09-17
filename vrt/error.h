#pragma once

#include <csetjmp>
#include <vrt/error.h>

namespace vrt
{
  /** Nested catch point for a runtime Error. */
  struct ErrorCatchPoint
  {
    ErrorCatchPoint* parent;
    std::jmp_buf continuation;
    ErrorInfo error{};
  };

  [[noreturn]] void raise_error(Error error);
  [[nodiscard]] ErrorInfo
  try_invoke(InvocationFunction function, void* user_context);
}
