#include "frame.h"

#include "failure.h"
#include "thread_context.h"
#include "region.h"

#include <limits>
#include <new>

extern "C" VRT_EXPORT vrt_frame* vrt_frame_enter(const vrt_func* func)
{
  auto& context = vrt::ThreadContext::get();
  auto& thread = context.thread;

  internal_check(
    ((thread.frame == nullptr) == (context.continuation == nullptr)) &&
      ((thread.frame == nullptr) ||
       (context.continuation->frame == thread.frame)),
    vrt::Failure::invalid_frame_state);

  auto frame_id = vrt::Location::stack();
  if (thread.frame != nullptr)
  {
    auto parent_id = thread.frame->frame_id;
    internal_check(
      parent_id.raw() <=
        (std::numeric_limits<uintptr_t>::max() - vrt::Location::FrameInc),
      vrt::Failure::invalid_frame_state);

    frame_id = parent_id.next_stack_level();
  }

  auto* frame = new (std::nothrow)
    vrt::Frame{thread.frame, nullptr, 0, 0, func, frame_id, frame_id};
  if (frame == nullptr)
    vrt::fail(vrt::Failure::out_of_memory);

  auto* continuation =
    new (std::nothrow) vrt::Continuation{context.continuation, frame};
  if (continuation == nullptr)
  {
    delete frame;
    vrt::fail(vrt::Failure::out_of_memory);
  }

  // Match the interpreter's Frame invariant: every logical frame owns an RC
  // region whose depth is the frame's stack depth.
  vrt::frame_region(frame);
  thread.frame = frame;
  context.continuation = continuation;
  return frame;
}

extern "C" VRT_EXPORT void vrt_frame_leave(void)
{
  auto& context = vrt::ThreadContext::get();
  auto& thread = context.thread;
  auto* frame = thread.frame;
  internal_check(frame != nullptr, vrt::Failure::invalid_frame_state);

  context.unwind_frames(frame->parent);
}

extern "C" VRT_EXPORT void vrt_frame_reuse(const vrt_func* func)
{
  auto& context = vrt::ThreadContext::get();
  auto& thread = context.thread;
  auto* frame = thread.frame;
  internal_check(
    (frame != nullptr) && (context.continuation != nullptr) &&
      (context.continuation->frame == frame) &&
      !context.continuation->raised_value.has_value(),
    vrt::Failure::invalid_frame_state);

  // Compiler-emitted Drop operations perform local register teardown. The
  // logical frame and its frame-local region survive a tailcall and are
  // reclaimed only when this frame is left or unwound.
  frame->func = func;
}

extern "C" VRT_EXPORT uint64_t vrt_frame_get_raise_target(void)
{
  auto* frame = vrt::ThreadContext::get().thread.frame;
  internal_check(frame != nullptr, vrt::Failure::invalid_frame_state);

  return frame->raise_target.raw();
}

extern "C" VRT_EXPORT uint64_t vrt_frame_set_raise_target(uint64_t target)
{
  auto* frame = vrt::ThreadContext::get().thread.frame;
  internal_check(frame != nullptr, vrt::Failure::invalid_frame_state);

  auto previous = frame->raise_target.raw();
  frame->raise_target = vrt::Location::from_raw(target);
  return previous;
}

extern "C" VRT_EXPORT void* vrt_frame_raise_continuation(void)
{
  auto& context = vrt::ThreadContext::get();
  auto* continuation = context.continuation;
  internal_check(
    (context.thread.frame != nullptr) && (continuation != nullptr) &&
      (continuation->frame == context.thread.frame) &&
      !continuation->raised_value.has_value(),
    vrt::Failure::invalid_frame_state);

  return continuation->state;
}

extern "C" VRT_EXPORT void vrt_frame_raise(uint64_t value)
{
  auto& context = vrt::ThreadContext::get();
  auto* frame = context.thread.frame;
  internal_check(frame != nullptr, vrt::Failure::invalid_frame_state);

  context.raise(value, frame->raise_target);
}

extern "C" VRT_EXPORT uint64_t vrt_frame_take_raised_value(void)
{
  auto& context = vrt::ThreadContext::get();
  auto* continuation = context.continuation;
  internal_check(
    (context.thread.frame != nullptr) && (continuation != nullptr) &&
      (continuation->frame == context.thread.frame) &&
      continuation->raised_value.has_value(),
    vrt::Failure::invalid_frame_state);

  auto value = *continuation->raised_value;
  continuation->raised_value.reset();
  return value;
}

extern "C" VRT_EXPORT vrt_frame* vrt_frame_parent(vrt_frame* frame)
{
  if (frame == nullptr)
    return nullptr;

  return frame->parent;
}

extern "C" VRT_EXPORT uint64_t vrt_frame_id(const vrt_frame* frame)
{
  if (frame == nullptr)
    return 0;

  return frame->frame_id.raw();
}

extern "C" VRT_EXPORT const vrt_func* vrt_frame_func(const vrt_frame* frame)
{
  if (frame == nullptr)
    return nullptr;

  return frame->func;
}
