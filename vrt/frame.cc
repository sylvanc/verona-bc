#include "frame.h"

#include "region.h"
#include "thread.h"

#include <cassert>
#include <csetjmp>
#include <exception>
#include <new>

namespace
{
  vrt_frame* current_frame()
  {
    auto* thread = vrt::current_thread();
    assert(thread != nullptr);

    if (thread == nullptr)
      std::terminate();

    auto* frame = thread->current_frame;
    assert(frame != nullptr);

    if (frame == nullptr)
      std::terminate();

    return frame;
  }
}

extern "C" VRT_EXPORT vrt_frame*
vrt_frame_enter(const vrt_function_descriptor* function)
{
  auto* thread = vrt::current_thread();
  assert(thread != nullptr);

  if (thread == nullptr)
    std::terminate();

  if (thread->next_frame_id == 0)
    std::terminate();

  auto* frame = new (std::nothrow) vrt_frame{
    thread->current_frame,
    nullptr,
    0,
    0,
    thread->next_frame_id,
    function,
    thread->next_frame_id};
  if (frame == nullptr)
    std::terminate();

  // Match the interpreter's Frame invariant: every logical frame owns an RC
  // region whose depth is the frame's stack depth.
  vrt::frame_region(frame);
  thread->next_frame_id++;
  thread->current_frame = frame;
  return frame;
}

extern "C" VRT_EXPORT void vrt_frame_leave(void)
{
  auto* thread = vrt::current_thread();
  assert(thread != nullptr);

  if (thread == nullptr)
    return;

  auto* frame = thread->current_frame;
  assert(frame != nullptr);

  if (frame == nullptr)
    return;

  auto* parent = frame->parent;
  vrt::destroy_frame_region(frame);
  thread->current_frame = parent;
  delete frame;
}

extern "C" VRT_EXPORT void
vrt_frame_reuse(const vrt_function_descriptor* function)
{
  auto* thread = vrt::current_thread();
  assert(thread != nullptr);

  if (thread == nullptr)
    return;

  auto* frame = thread->current_frame;
  assert(frame != nullptr);

  if (frame == nullptr)
    return;

  // Compiler-emitted Drop operations perform local register teardown. The
  // logical frame and its frame-local region survive a tailcall and are
  // reclaimed only when this frame is left or unwound.
  frame->function = function;
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
  assert(thread != nullptr);

  if (thread == nullptr)
    std::terminate();

  auto* current = thread->current_frame;
  if (current == nullptr)
    std::terminate();

  auto* target = current->parent;
  while ((target != nullptr) && (target->frame_id != current->raise_target))
    target = target->parent;

  if (target == nullptr)
    std::terminate();

  thread->pending_raise_value = value;
  thread->pending_raise_target = target;
  thread->raise_pending = true;

  while (thread->current_frame != target)
  {
    auto* frame = thread->current_frame;
    auto* parent = frame->parent;
    vrt::destroy_frame_region(frame);
    thread->current_frame = parent;
    delete frame;
  }

  std::longjmp(target->raise_continuation, 1);
}

extern "C" VRT_EXPORT uint64_t vrt_frame_take_raised_value(void)
{
  auto* thread = vrt::current_thread();
  if (
    (thread == nullptr) || !thread->raise_pending ||
    (thread->current_frame != thread->pending_raise_target))
    std::terminate();

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

extern "C" VRT_EXPORT const vrt_function_descriptor*
vrt_frame_function(const vrt_frame* frame)
{
  if (frame == nullptr)
    return nullptr;

  return frame->function;
}
