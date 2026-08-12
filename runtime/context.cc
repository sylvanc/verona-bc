#include "context.h"

#include <cassert>
#include <new>

extern "C" VRT_EXPORT vrt_thread* vrt_thread_create(void)
{
  return new (std::nothrow) vrt_thread{};
}

extern "C" VRT_EXPORT void vrt_thread_destroy(vrt_thread* thread)
{
  if (thread == nullptr)
    return;

  while (thread->current_frame != nullptr)
  {
    auto* frame = thread->current_frame;
    thread->current_frame = frame->parent;
    delete frame;
  }

  delete thread;
}

extern "C" VRT_EXPORT vrt_frame* vrt_thread_current_frame(vrt_thread* thread)
{
  if (thread == nullptr)
    return nullptr;

  return thread->current_frame;
}

extern "C" VRT_EXPORT vrt_frame*
vrt_frame_enter(vrt_thread* thread, const vrt_function_descriptor* function)
{
  if ((thread == nullptr) || (thread->next_frame_id == 0))
    return nullptr;

  auto* frame = new (std::nothrow) vrt_frame{
    thread,
    thread->current_frame,
    nullptr,
    0,
    0,
    thread->next_frame_id,
    function,
    0};
  if (frame == nullptr)
    return nullptr;

  thread->next_frame_id++;
  thread->current_frame = frame;
  return frame;
}

extern "C" VRT_EXPORT void vrt_frame_leave(vrt_thread* thread, vrt_frame* frame)
{
  assert(thread != nullptr);
  assert(frame != nullptr);
  assert(frame->thread == thread);
  assert(thread->current_frame == frame);

  if (
    (thread == nullptr) || (frame == nullptr) || (frame->thread != thread) ||
    (thread->current_frame != frame))
    return;

  thread->current_frame = frame->parent;
  delete frame;
}

extern "C" VRT_EXPORT void vrt_frame_prepare_tailcall(
  vrt_thread* thread, vrt_frame* frame, const vrt_function_descriptor* function)
{
  assert(thread != nullptr);
  assert(frame != nullptr);
  assert(frame->thread == thread);
  assert(thread->current_frame == frame);

  if (
    (thread == nullptr) || (frame == nullptr) || (frame->thread != thread) ||
    (thread->current_frame != frame))
    return;

  frame->function = function;
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
