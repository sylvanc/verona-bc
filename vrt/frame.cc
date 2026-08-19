#include "frame.h"

#include "thread.h"

#include <cassert>
#include <exception>
#include <new>

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
    0};
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
