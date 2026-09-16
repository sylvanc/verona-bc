#pragma once

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

  [[noreturn]] void fail(Failure reason);
}
