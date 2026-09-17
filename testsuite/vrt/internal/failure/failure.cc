#include "failure.h"

#include <cstring>

int main()
{
  const struct
  {
    vrt::Failure failure;
    const char* message;
  } failure_messages[] = {
    {vrt::Failure::invalid_array_state,
     "invalid array state: array metadata, bounds, or element operation "
     "violated a runtime invariant"},
    {vrt::Failure::invalid_error_state,
     "invalid error state: the active error or unwind state is inconsistent"},
    {vrt::Failure::invalid_frame_state,
     "invalid frame state: the frame stack or frame metadata is inconsistent"},
    {vrt::Failure::invalid_function_state,
     "invalid function state: the function metadata is invalid"},
    {vrt::Failure::invalid_header_state,
     "invalid header state: object header metadata is invalid"},
    {vrt::Failure::invalid_object_state,
     "invalid object state: object metadata, fields, or ownership violated a "
     "runtime invariant"},
    {vrt::Failure::invalid_program_state,
     "invalid program state: program metadata or lifecycle state is "
     "inconsistent"},
    {vrt::Failure::invalid_region_state,
     "invalid region state: region metadata, membership, or ownership "
     "violated a runtime invariant"},
    {vrt::Failure::invalid_runtime_state,
     "invalid runtime state: runtime initialization or execution state is "
     "inconsistent"},
    {vrt::Failure::invalid_thread_state,
     "invalid thread state: thread-local runtime state is inconsistent"},
    {vrt::Failure::invalid_value_state,
     "invalid value state: the runtime value tag or payload is invalid"},
    {vrt::Failure::invalid_write,
     "invalid write: the write barrier rejected an ownership or region "
     "transition"},
    {vrt::Failure::out_of_memory,
     "out of memory: a runtime allocation failed"},
  };

  for (const auto& [failure, expected] : failure_messages)
  {
    if (std::strcmp(vrt::failure_message(failure), expected) != 0)
      return 1;
  }

  return 0;
}