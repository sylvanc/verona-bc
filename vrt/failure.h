#pragma once

#include <source_location>

namespace vrt
{
  enum class Failure
  {
    invalid_array_state,
    invalid_error_state,
    invalid_frame_state,
    invalid_function_state,
    invalid_header_state,
    invalid_object_state,
    invalid_program_state,
    invalid_region_state,
    invalid_runtime_state,
    invalid_thread_state,
    invalid_value_state,
    invalid_write,
    out_of_memory,
  };

  [[nodiscard]] const char* failure_message(Failure reason) noexcept;

  [[noreturn]] void fail(
    Failure reason,
    std::source_location location = std::source_location::current());

  void internal_check_impl(
    bool guard,
    Failure reason,
    std::source_location location = std::source_location::current());
}

#ifdef VRT_ENABLE_INTERNAL_CHECKS
#  define internal_check(guard, code) \
    ::vrt::internal_check_impl((guard), (code))
#else
#  define internal_check(guard, code) \
  ((void)sizeof(guard), (void)sizeof(code))
#endif
