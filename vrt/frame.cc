#include "frame.h"

#include "thread.h"

#include <cassert>
#include <csetjmp>
#include <exception>
#include <new>

namespace
{
  vrt_frame* current_stable_frame()
  {
    auto* thread = vrt::current_thread();
    assert(thread != nullptr);
    assert((thread == nullptr) || !thread->tailcall_pending);

    if ((thread == nullptr) || thread->tailcall_pending)
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

  if (thread->tailcall_pending)
  {
    auto* frame = thread->current_frame;
    assert(frame != nullptr);
    assert(
      (thread->tailcall_target == nullptr) ||
      (thread->tailcall_target == function));

    if (
      (frame == nullptr) ||
      ((thread->tailcall_target != nullptr) &&
       (thread->tailcall_target != function)))
      std::terminate();

    thread->tailcall_pending = false;
    thread->tailcall_target = nullptr;
    frame->function = function;
    return frame;
  }

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

  thread->next_frame_id++;
  thread->current_frame = frame;
  return frame;
}

extern "C" VRT_EXPORT void vrt_frame_leave(void)
{
  auto* thread = vrt::current_thread();
  assert(thread != nullptr);
  assert(!thread->tailcall_pending);

  if ((thread == nullptr) || thread->tailcall_pending)
    return;

  auto* frame = thread->current_frame;
  assert(frame != nullptr);

  if (frame == nullptr)
    return;

  thread->current_frame = frame->parent;
  delete frame;
}

extern "C" VRT_EXPORT void
vrt_frame_prepare_tailcall(const vrt_function_descriptor* function)
{
  auto* thread = vrt::current_thread();
  assert(thread != nullptr);
  assert(!thread->tailcall_pending);

  if ((thread == nullptr) || thread->tailcall_pending)
    return;

  auto* frame = thread->current_frame;
  assert(frame != nullptr);

  if (frame == nullptr)
    return;

  frame->function = function;
  thread->tailcall_target = function;
  thread->tailcall_pending = true;
}

extern "C" VRT_EXPORT uint64_t vrt_frame_get_raise_target(void)
{
  return current_stable_frame()->raise_target;
}

extern "C" VRT_EXPORT uint64_t vrt_frame_set_raise_target(uint64_t target)
{
  auto* frame = current_stable_frame();
  auto previous = frame->raise_target;
  frame->raise_target = target;
  return previous;
}

extern "C" VRT_EXPORT void* vrt_frame_raise_continuation(vrt_frame* frame)
{
  if (frame == nullptr)
    return nullptr;

  return frame->raise_continuation;
}

extern "C" VRT_EXPORT void vrt_frame_raise(uint64_t value)
{
  auto* thread = vrt::current_thread();
  assert(thread != nullptr);
  assert(!thread->tailcall_pending);

  if ((thread == nullptr) || thread->tailcall_pending)
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
    thread->current_frame = frame->parent;
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
