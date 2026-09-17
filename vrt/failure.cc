#include "failure.h"

#include <cstdio>
#include <exception>

namespace vrt
{
  const char* failure_message(Failure reason) noexcept
  {
    switch (reason)
    {
      case Failure::invalid_array_state:
        return "invalid array state: array metadata, bounds, or element "
               "operation violated a runtime invariant";
      case Failure::invalid_error_state:
        return "invalid error state: the active error or unwind state is "
               "inconsistent";
      case Failure::invalid_frame_state:
        return "invalid frame state: the frame stack or frame metadata is "
               "inconsistent";
      case Failure::invalid_function_state:
        return "invalid function state: the function metadata is invalid";
      case Failure::invalid_header_state:
        return "invalid header state: object header metadata is invalid";
      case Failure::invalid_object_state:
        return "invalid object state: object metadata, fields, or ownership "
               "violated a runtime invariant";
      case Failure::invalid_program_state:
        return "invalid program state: program metadata or lifecycle state "
               "is inconsistent";
      case Failure::invalid_region_state:
        return "invalid region state: region metadata, membership, or "
               "ownership violated a runtime invariant";
      case Failure::invalid_runtime_state:
        return "invalid runtime state: runtime initialization or execution "
               "state is inconsistent";
      case Failure::invalid_thread_state:
        return "invalid thread state: thread-local runtime state is "
               "inconsistent";
      case Failure::invalid_value_state:
        return "invalid value state: the runtime value tag or payload is "
               "invalid";
      case Failure::invalid_write:
        return "invalid write: the write barrier rejected an ownership or "
               "region transition";
      case Failure::out_of_memory:
        return "out of memory: a runtime allocation failed";
    }

    return "unknown runtime failure";
  }

  [[noreturn]] void fail(Failure reason, std::source_location location)
  {
    std::fprintf(
      stderr,
      "VRT failure at %s:%lu in %s: %s\n",
      location.file_name(),
      static_cast<unsigned long>(location.line()),
      location.function_name(),
      failure_message(reason));
    std::fflush(stderr);
    std::terminate();
  }

  void internal_check_impl(
    bool guard, Failure reason, std::source_location location)
  {
    if (!guard)
      fail(reason, location);
  }
}
