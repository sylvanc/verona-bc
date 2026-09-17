#include "error.h"

#include "failure.h"
#include "frame.h"
#include "thread_context.h"

#include <csetjmp>
#include <new>

namespace
{
  const char* error_message(vrt::Error error)
  {
    switch (error)
    {
      case vrt::Error::none:
        return "no error";
      case vrt::Error::bad_raise_target:
        return "bad raise target";
      case vrt::Error::bad_alloc_target:
        return "bad alloc target";
      case vrt::Error::bad_array_index:
        return "bad array index";
      case vrt::Error::bad_store_target:
        return "bad store target";
      case vrt::Error::bad_store:
        return "bad store";
      case vrt::Error::method_not_found:
        return "method not found";
      case vrt::Error::bad_stack_escape:
        return "bad stack escape";
      case vrt::Error::bad_region_entry_point:
        return "bad region entry point";
      case vrt::Error::bad_freeze:
        return "cannot freeze a readonly value";
      case vrt::Error::bad_merge:
        return "cannot merge regions: both have owners";
      case vrt::Error::scheduler_already_running:
        return "scheduler already running";
    }

    vrt::fail(vrt::Failure::invalid_error_state);
  }
}

namespace vrt
{
  [[noreturn]] void ThreadContext::raise_error(Error error)
  {
    internal_check(
      (error != Error::none) && (error_catch_point != nullptr) &&
        (error_catch_point->error.code == Error::none),
      Failure::invalid_error_state);

    auto* catch_point = error_catch_point;
    catch_point->error = {
      error, thread.frame == nullptr ? nullptr : thread.frame->func, 0};
    unwind_frames(nullptr);
    std::longjmp(catch_point->continuation, 1);
  }

  ErrorInfo ThreadContext::try_invoke(
    InvocationFunction function, void* user_context)
  {
    internal_check(
      (function != nullptr) && (thread.frame == nullptr) &&
        (continuation == nullptr),
      Failure::invalid_error_state);

    auto* catch_point =
      new (std::nothrow) ErrorCatchPoint{error_catch_point, {}, {}};
    if (catch_point == nullptr)
      fail(Failure::out_of_memory);

    error_catch_point = catch_point;

    if (setjmp(catch_point->continuation) == 0)
    {
      function(user_context);
      internal_check(
        (thread.frame == nullptr) && (continuation == nullptr),
        Failure::invalid_frame_state);

      internal_check(
        (error_catch_point == catch_point) &&
          (catch_point->error.code == Error::none),
        Failure::invalid_error_state);

      error_catch_point = catch_point->parent;
      delete catch_point;
      return {};
    }

    internal_check(
      (error_catch_point == catch_point) &&
        (catch_point->error.code != Error::none),
      Failure::invalid_error_state);

    auto error = catch_point->error;
    error_catch_point = catch_point->parent;
    delete catch_point;
    return error;
  }

  [[noreturn]] void raise_error(Error error)
  {
    auto* context = ThreadContext::try_get();
    internal_check(context != nullptr, Failure::invalid_error_state);

    context->raise_error(error);
  }

  ErrorInfo try_invoke(InvocationFunction function, void* user_context)
  {
    auto* context = ThreadContext::try_get();
    internal_check(context != nullptr, Failure::invalid_error_state);

    return context->try_invoke(function, user_context);
  }
}

extern "C" VRT_EXPORT const char* vrt_error_message(vrt::Error error)
{
  return error_message(error);
}

extern "C" VRT_EXPORT int vrt_try_invoke(
  vrt::InvocationFunction function, void* user_context, vrt::ErrorInfo* error)
{
  internal_check(error != nullptr, vrt::Failure::invalid_error_state);

  *error = vrt::try_invoke(function, user_context);
  return error->code == vrt::Error::none;
}

extern "C" VRT_EXPORT void vrt_error_raise(vrt::Error error)
{
  vrt::raise_error(error);
}
