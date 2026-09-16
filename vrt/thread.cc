#include "thread.h"

#include "failure.h"
#include "frame.h"
#include "region.h"
#include "vrt.h"

#include <new>

namespace
{
  thread_local vrt::Thread* current_thread_state = nullptr;

  void destroy_frames(vrt::Thread* thread)
  {
    while (thread->frame != nullptr)
    {
      auto* frame = thread->frame;
      auto* parent = frame->parent;
      vrt::destroy_frame_region(frame);
      thread->frame = parent;
      delete frame;
    }
  }
}

namespace vrt
{
  Thread* current_thread()
  {
    return current_thread_state;
  }

  void init_thread()
  {
    if (current_thread_state != nullptr)
      fail(Failure::invalid_thread_state);

    current_thread_state = new (std::nothrow) Thread{};

    if (current_thread_state == nullptr)
      fail(Failure::out_of_memory);
  }

  void deinit_thread()
  {
    if (current_thread_state == nullptr)
      fail(Failure::invalid_thread_state);

    destroy_frames(current_thread_state);
    delete current_thread_state;
    current_thread_state = nullptr;
  }
}

extern "C" VRT_EXPORT vrt_thread* vrt_thread_current(void)
{
  return vrt::current_thread();
}

extern "C" VRT_EXPORT vrt_frame* vrt_thread_current_frame(void)
{
  auto* thread = vrt::current_thread();
  if (thread == nullptr)
    return nullptr;

  return thread->frame;
}
