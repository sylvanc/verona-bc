#include "thread_context.h"

#include "failure.h"
#include "frame.h"
#include "region.h"

#include <optional>

namespace
{
  std::optional<vrt::ThreadContext>& context_slot()
  {
    // VBCI owns its execution context in function-local Thread TLS.
    // The optional preserves VRT's explicit thread init/deinit lifecycle.
    static thread_local std::optional<vrt::ThreadContext> context;
    return context;
  }
}

namespace vrt
{
  ThreadContext& ThreadContext::get()
  {
    auto* context = try_get();
    internal_check(context != nullptr, Failure::invalid_thread_state);

    return *context;
  }

  ThreadContext* ThreadContext::try_get()
  {
    auto& context = context_slot();
    return context.has_value() ? &*context : nullptr;
  }

  void ThreadContext::init()
  {
    auto& context = context_slot();
    internal_check(!context.has_value(), Failure::invalid_thread_state);

    context.emplace();
  }

  void ThreadContext::deinit()
  {
    auto& context = context_slot();
    internal_check(context.has_value(), Failure::invalid_thread_state);

    context->unwind_frames(nullptr);
    context.reset();
  }

  [[noreturn]] void ThreadContext::raise(uint64_t value, Location target_id)
  {
    auto* frame = thread.frame;
    internal_check(frame != nullptr, Failure::invalid_thread_state);

    internal_check(
      target_id.is_stack() && (target_id < frame->frame_id),
      Failure::invalid_frame_state);

    auto* target = frame->parent;
    while ((target != nullptr) && (target->frame_id != target_id))
      target = target->parent;

    internal_check(target != nullptr, Failure::invalid_frame_state);

    unwind_frames(target);

    auto* target_continuation = continuation;
    internal_check(
      (target_continuation != nullptr) &&
        (target_continuation->frame == target) && (thread.frame == target) &&
        !target_continuation->raised_value.has_value(),
      Failure::invalid_frame_state);

    target_continuation->raised_value = value;
    std::longjmp(target_continuation->state, 1);
  }

  void ThreadContext::unwind_frames(Frame* target)
  {
    while (thread.frame != target)
    {
      auto* frame = thread.frame;
      auto* current_continuation = continuation;
      internal_check(
        (frame != nullptr) && (current_continuation != nullptr) &&
          (current_continuation->frame == frame) &&
          !current_continuation->raised_value.has_value(),
        Failure::invalid_frame_state);

      auto* parent = frame->parent;
      destroy_frame_region(frame);
      thread.frame = parent;
      continuation = current_continuation->parent;
      delete current_continuation;
      delete frame;
    }

    internal_check(
      (target == nullptr) ?
        (continuation == nullptr) :
        ((continuation != nullptr) && (continuation->frame == target)),
      Failure::invalid_frame_state);
  }
}