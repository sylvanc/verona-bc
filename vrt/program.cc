#include "failure.h"
#include "thread_context.h"
#include "vrt.h"

#include <mutex>
#include <new>
#include <unordered_set>
#include <vrt/program.h>

namespace
{
  std::mutex lifecycle_mutex;
  bool runtime_initialized = false;
  std::unordered_set<const vrt::Program*> initialized_programs;

  void init_runtime_services()
  {
    // TODO: Initialize process-wide scheduler and runtime services here.
  }

  void init_program_state(const vrt::Program& program)
  {
    if (
      (program.type_count != 0) || (program.types != nullptr) ||
      (program.singleton_count != 0) || (program.singletons != nullptr))
      vrt::fail(vrt::Failure::invalid_program_state);

    // TODO: Register compiler-emitted types, initialize singleton headers and
    // memo slots, then run FFI initializers. The ordering belongs here rather
    // than in each invocation.
  }
}

extern "C" VRT_EXPORT void vrt_runtime_init(void)
{
  std::lock_guard guard(lifecycle_mutex);
  if (runtime_initialized)
    vrt::fail(vrt::Failure::invalid_runtime_state);

  init_runtime_services();
  runtime_initialized = true;
}

extern "C" VRT_EXPORT void vrt_program_init(const vrt::Program* program)
{
  if (program == nullptr)
    vrt::fail(vrt::Failure::invalid_program_state);

  std::lock_guard guard(lifecycle_mutex);
  if (!runtime_initialized)
    vrt::fail(vrt::Failure::invalid_runtime_state);

  if (initialized_programs.contains(program))
    vrt::fail(vrt::Failure::invalid_program_state);

  init_program_state(*program);

  try
  {
    initialized_programs.insert(program);
  }
  catch (const std::bad_alloc&)
  {
    vrt::fail(vrt::Failure::out_of_memory);
  }
}

extern "C" VRT_EXPORT void vrt_invocation_begin(void)
{
  {
    std::lock_guard guard(lifecycle_mutex);
    if (!runtime_initialized || initialized_programs.empty())
      vrt::fail(vrt::Failure::invalid_runtime_state);
  }

  auto& context = vrt::ThreadContext::get();
  if (
    (context.thread.frame != nullptr) || (context.continuation != nullptr) ||
    (context.error_catch_point != nullptr))
    vrt::fail(vrt::Failure::invalid_thread_state);

  vrt::reset_exit_code();
}
