#include "frame.h"

#include "failure.h"
#include "region.h"
#include "thread.h"

#include <csetjmp>
#include <new>

namespace
{
  vrt::Frame* current_frame()
  {
    auto* thread = vrt::current_thread();
    if (thread == nullptr)
      vrt::fail(vrt::Failure::invalid_frame_state);

    auto* frame = thread->frame;
    if (frame == nullptr)
      vrt::fail(vrt::Failure::invalid_frame_state);

    return frame;
  }
}

extern "C" VRT_EXPORT vrt_frame*
vrt_frame_enter(const vrt_func* func)
{
  auto* thread = vrt::current_thread();
  if (thread == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  if (thread->next_frame_id == 0)
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto* frame = new (std::nothrow) vrt::Frame{
    thread->frame,
    nullptr,
    0,
    0,
    thread->next_frame_id,
    func,
    thread->next_frame_id};
  if (frame == nullptr)
    vrt::fail(vrt::Failure::out_of_memory);

  // Match the interpreter's Frame invariant: every logical frame owns an RC
  // region whose depth is the frame's stack depth.
  vrt::frame_region(frame);
  thread->next_frame_id++;
  thread->frame = frame;
  return frame;
}

extern "C" VRT_EXPORT void vrt_frame_leave(void)
{
  auto* thread = vrt::current_thread();
  if (thread == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto* frame = thread->frame;
  if (frame == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto* parent = frame->parent;
  vrt::destroy_frame_region(frame);
  thread->frame = parent;
  delete frame;
}

extern "C" VRT_EXPORT void
vrt_frame_reuse(const vrt_func* func)
{
  auto* thread = vrt::current_thread();
  if (thread == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto* frame = thread->frame;
  if (frame == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  // Compiler-emitted Drop operations perform local register teardown. The
  // logical frame and its frame-local region survive a tailcall and are
  // reclaimed only when this frame is left or unwound.
  frame->func = func;
}

extern "C" VRT_EXPORT uint64_t vrt_frame_get_raise_target(void)
{
  return current_frame()->raise_target;
}

extern "C" VRT_EXPORT uint64_t vrt_frame_set_raise_target(uint64_t target)
{
  auto* frame = current_frame();
  auto previous = frame->raise_target;
  frame->raise_target = target;
  return previous;
}

extern "C" VRT_EXPORT void* vrt_frame_raise_continuation(void)
{
  return current_frame()->raise_continuation;
}

extern "C" VRT_EXPORT void vrt_frame_raise(uint64_t value)
{
  auto* thread = vrt::current_thread();
  if (thread == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto* current = thread->frame;
  if (current == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto* target = current->parent;
  while ((target != nullptr) && (target->frame_id != current->raise_target))
    target = target->parent;

  if (target == nullptr)
    vrt::fail(vrt::Failure::invalid_frame_state);

  thread->pending_raise_value = value;
  thread->pending_raise_target = target;
  thread->raise_pending = true;

  while (thread->frame != target)
  {
    auto* frame = thread->frame;
    auto* parent = frame->parent;
    vrt::destroy_frame_region(frame);
    thread->frame = parent;
    delete frame;
  }

  std::longjmp(target->raise_continuation, 1);
}

extern "C" VRT_EXPORT uint64_t vrt_frame_take_raised_value(void)
{
  auto* thread = vrt::current_thread();
  if (
    (thread == nullptr) || !thread->raise_pending ||
    (thread->frame != thread->pending_raise_target))
    vrt::fail(vrt::Failure::invalid_frame_state);

  auto value = thread->pending_raise_value;
  thread->pending_raise_value = 0;
  thread->pending_raise_target = nullptr;
  thread->raise_pending = false;
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

  return frame->frame_id;
}

extern "C" VRT_EXPORT const vrt_func* vrt_frame_func(const vrt_frame* frame)
{
  if (frame == nullptr)
    return nullptr;

  return frame->func;
}
