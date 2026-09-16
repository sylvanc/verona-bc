#include "thread.h"

#include "failure.h"
#include "frame.h"
#include "native_context.h"
#include "vrt.h"

#include <csetjmp>

namespace vrt
{
  Thread& Thread::get()
  {
    return NativeContext::get().thread;
  }

  Thread* Thread::try_get()
  {
    auto* context = NativeContext::try_get();
    return context == nullptr ? nullptr : &context->thread;
  }

  [[noreturn]] void Thread::raise(uint64_t value, Location target_id)
  {
    auto& context = NativeContext::get();
    if ((&context.thread != this) || (frame == nullptr))
      fail(Failure::invalid_thread_state);

    if (!target_id.is_stack() || (target_id >= frame->frame_id))
      fail(Failure::invalid_frame_state);

    auto* target = frame->parent;
    while ((target != nullptr) && (target->frame_id != target_id))
      target = target->parent;

    if (target == nullptr)
      fail(Failure::invalid_frame_state);

    unwind_frames(context, target);

    auto* continuation = context.continuation;
    if (
      (continuation == nullptr) || (continuation->frame != target) ||
      (frame != target) || continuation->raised_value.has_value())
      fail(Failure::invalid_frame_state);

    continuation->raised_value = value;
    std::longjmp(continuation->state, 1);
  }

  void init_thread()
  {
    NativeContext::init();
  }

  void deinit_thread()
  {
    NativeContext::deinit();
  }
}

extern "C" VRT_EXPORT vrt_thread* vrt_thread_current(void)
{
  return vrt::Thread::try_get();
}

extern "C" VRT_EXPORT vrt_frame* vrt_thread_current_frame(void)
{
  auto* thread = vrt::Thread::try_get();
  if (thread == nullptr)
    return nullptr;

  return thread->frame;
}
