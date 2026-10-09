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
        return "cannot freeze stack or arena value";
      case vrt::Error::bad_merge:
        return "cannot merge regions: both have owners";
      case vrt::Error::scheduler_already_running:
        return "scheduler already running";
      case vrt::Error::bad_reference_target:
        return "bad reference target";
    }

    vrt::fail(vrt::Failure::invalid_error_state);
  }
}

namespace vrt
{
  [[noreturn]] void ThreadContext::raise_error(Error error)
  {
    internal_check(
      (error != Error::none) && (error_boundary != nullptr) &&
        (error_boundary->error.code == Error::none),
      Failure::invalid_error_state);

    auto* boundary = error_boundary;
    boundary->error = {
      error, thread.frame == nullptr ? nullptr : thread.frame->func, 0};
    unwind_frames(boundary->frame_boundary.frame);
    internal_check(
      continuation == boundary->frame_boundary.continuation,
      Failure::invalid_error_state);
    std::longjmp(boundary->recovery, 1);
  }

  ErrorInfo
  ThreadContext::try_invoke(InvocationFunction function, void* user_context)
  {
    internal_check(
      (function != nullptr) && (thread.frame == nullptr) &&
        (continuation == nullptr),
      Failure::invalid_error_state);

    auto* boundary = new (std::nothrow)
      ErrorBoundary{error_boundary, {nullptr, nullptr}, {}, {}};
    if (boundary == nullptr)
      fail(Failure::out_of_memory);

    error_boundary = boundary;

    if (setjmp(boundary->recovery) == 0)
    {
      function(user_context);
      internal_check(
        (thread.frame == nullptr) && (continuation == nullptr),
        Failure::invalid_frame_state);

      internal_check(
        (error_boundary == boundary) && (boundary->error.code == Error::none),
        Failure::invalid_error_state);

      error_boundary = boundary->parent;
      delete boundary;
      return {};
    }

    internal_check(
      (error_boundary == boundary) && (boundary->error.code != Error::none),
      Failure::invalid_error_state);

    auto error = boundary->error;
    error_boundary = boundary->parent;
    delete boundary;
    return error;
  }

  ErrorInfo
  ThreadContext::run_cleanup(InvocationFunction function, void* user_context)
  {
    internal_check(function != nullptr, Failure::invalid_error_state);
    const FrameBoundary frame_boundary{thread.frame, continuation};
    internal_check(
      (frame_boundary.frame == nullptr) ?
        (frame_boundary.continuation == nullptr) :
        ((frame_boundary.continuation != nullptr) &&
         (frame_boundary.continuation->frame == frame_boundary.frame)),
      Failure::invalid_frame_state);

    auto* boundary =
      new (std::nothrow) ErrorBoundary{error_boundary, frame_boundary, {}, {}};
    if (boundary == nullptr)
      fail(Failure::out_of_memory);

    error_boundary = boundary;
    if (setjmp(boundary->recovery) == 0)
    {
      function(user_context);
      internal_check(
        (thread.frame == boundary->frame_boundary.frame) &&
          (continuation == boundary->frame_boundary.continuation),
        Failure::invalid_frame_state);
      internal_check(
        (error_boundary == boundary) && (boundary->error.code == Error::none),
        Failure::invalid_error_state);

      error_boundary = boundary->parent;
      delete boundary;
      return {};
    }

    internal_check(
      (thread.frame == boundary->frame_boundary.frame) &&
        (continuation == boundary->frame_boundary.continuation) &&
        (error_boundary == boundary) && (boundary->error.code != Error::none),
      Failure::invalid_error_state);

    auto error = boundary->error;
    error_boundary = boundary->parent;
    delete boundary;
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
