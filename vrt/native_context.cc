#include "native_context.h"

#include "failure.h"
#include "frame.h"
#include "region.h"

#include <optional>

namespace
{
  std::optional<vrt::NativeContext>& context_slot()
  {
    // VBCI's Thread::get() owns its execution context in function-local TLS.
    // The optional preserves VRT's explicit thread init/deinit lifecycle.
    static thread_local std::optional<vrt::NativeContext> context;
    return context;
  }
}

namespace vrt
{
  NativeContext& NativeContext::get()
  {
    auto* context = try_get();
    if (context == nullptr)
      fail(Failure::invalid_thread_state);

    return *context;
  }

  NativeContext* NativeContext::try_get()
  {
    auto& context = context_slot();
    return context.has_value() ? &*context : nullptr;
  }

  void NativeContext::init()
  {
    auto& context = context_slot();
    if (context.has_value())
      fail(Failure::invalid_thread_state);

    context.emplace();
  }

  void NativeContext::deinit()
  {
    auto& context = context_slot();
    if (!context.has_value())
      fail(Failure::invalid_thread_state);

    unwind_frames(*context, nullptr);
    context.reset();
  }

  void unwind_frames(NativeContext& context, Frame* target)
  {
    auto& thread = context.thread;
    while (thread.frame != target)
    {
      auto* frame = thread.frame;
      auto* continuation = context.continuation;
      if (
        (frame == nullptr) || (continuation == nullptr) ||
        (continuation->frame != frame) ||
        continuation->raised_value.has_value())
        fail(Failure::invalid_frame_state);

      auto* parent = frame->parent;
      destroy_frame_region(frame);
      thread.frame = parent;
      context.continuation = continuation->parent;
      delete continuation;
      delete frame;
    }

    if (
      ((target == nullptr) && (context.continuation != nullptr)) ||
      ((target != nullptr) &&
       ((context.continuation == nullptr) ||
        (context.continuation->frame != target))))
      fail(Failure::invalid_frame_state);
  }
}
