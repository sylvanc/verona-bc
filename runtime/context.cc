#include "context.h"

#include "vrt.h"

#include <cassert>
#include <exception>
#include <new>

namespace
{
  thread_local vrt_thread* current_thread = nullptr;

  void destroy_frames(vrt_thread* thread)
  {
    while (thread->current_frame != nullptr)
    {
      auto* frame = thread->current_frame;
      thread->current_frame = frame->parent;
      delete frame;
    }
  }
}

namespace vrt
{
  void init_thread()
  {
    assert(current_thread == nullptr);

    if (current_thread != nullptr)
      return;

    current_thread = new (std::nothrow) vrt_thread{};

    if (current_thread == nullptr)
      std::terminate();
  }

  void deinit_thread()
  {
    assert(current_thread != nullptr);

    if (current_thread == nullptr)
      return;

    destroy_frames(current_thread);
    delete current_thread;
    current_thread = nullptr;
  }
}

extern "C" VRT_EXPORT vrt_thread* vrt_thread_current(void)
{
  return current_thread;
}

extern "C" VRT_EXPORT vrt_frame* vrt_thread_current_frame(void)
{
  if (current_thread == nullptr)
    return nullptr;

  return current_thread->current_frame;
}

extern "C" VRT_EXPORT vrt_frame*
vrt_frame_enter(const vrt_function_descriptor* function)
{
  auto* thread = current_thread;
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
    thread->current_frame, nullptr, 0, 0, thread->next_frame_id, function, 0};
  if (frame == nullptr)
    std::terminate();

  thread->next_frame_id++;
  thread->current_frame = frame;
  return frame;
}

extern "C" VRT_EXPORT void vrt_frame_leave(void)
{
  auto* thread = current_thread;
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
  auto* thread = current_thread;
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
