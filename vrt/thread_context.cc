#include "thread_context.h"

#include "failure.h"
#include "frame.h"
#include "header.h"
#include "region.h"
#include "value.h"
#include "writebarrier.h"

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

  void ThreadContext::escape(Header* header)
  {
    internal_check(header != nullptr, Failure::invalid_header_state);

    if (header->location().is_immortal())
      return;

    internal_check(thread.frame != nullptr, Failure::invalid_header_state);

    auto* frame = thread.frame;
    auto* source = header->region();
    if ((source == nullptr) || !source->is_frame_local())
      return;

    if (frame->region != source)
      return;

    if (frame->parent != nullptr)
    {
      if (!writebarrier::drag(frame_region(frame->parent), header, false))
        raise_error(Error::bad_stack_escape);

      return;
    }

    auto* destination = Region::create(RegionType::rc);
    if (!writebarrier::drag(destination, header, false))
    {
      destroy_region(destination);
      raise_error(Error::bad_stack_escape);
    }
  }

  [[noreturn]] void ThreadContext::raise(
    ValueType value_type, uint64_t value, Location target_id)
  {
    auto* frame = thread.frame;
    internal_check(frame != nullptr, Failure::invalid_thread_state);

    if (!target_id.is_stack() || (target_id >= frame->frame_id))
      raise_error(Error::bad_raise_target);

    auto* target = frame->parent;
    while ((target != nullptr) && (target->frame_id != target_id))
      target = target->parent;

    if (target == nullptr)
      raise_error(Error::bad_raise_target);

    if (
      (value_type == ValueType::object) ||
      (value_type == ValueType::array))
    {
      auto* payload =
        reinterpret_cast<void*>(static_cast<uintptr_t>(value));
      auto* header = Value{value_type, payload}.header();
      auto* source = header->region();

      if ((source != nullptr) && source->is_frame_local())
      {
        auto* destination = frame_region(target);
        if (
          (source != destination) &&
          (source->frame_depth > destination->frame_depth) &&
          !writebarrier::drag(destination, header, false))
          raise_error(Error::bad_stack_escape);
      }
    }

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
