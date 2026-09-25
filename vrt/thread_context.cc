#include "thread_context.h"

#include "drag.h"
#include "failure.h"
#include "frame.h"
#include "header.h"
#include "program.h"
#include "reference.h"
#include "region.h"
#include "value.h"

#include <cstring>
#include <new>
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
      if (!drag_allocation(
        frame_region(frame->parent),
        header,
        {.root_reference = RootReference::retained}))
        raise_error(Error::bad_stack_escape);

      return;
    }

    auto* destination = Region::create(RegionType::rc);
        if (!drag_allocation(
          destination,
          header,
          {.root_reference = RootReference::retained}))
    {
      destroy_region(destination);
      raise_error(Error::bad_stack_escape);
    }
  }

  [[noreturn]] void
  ThreadContext::raise(
    uintptr_t type_id, const void* value_storage, Location target_id)
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

    auto* target_continuation = continuation;
    while (
      (target_continuation != nullptr) &&
      (target_continuation->frame != target))
      target_continuation = target_continuation->parent;

    internal_check(
      (target_continuation != nullptr) &&
        !target_continuation->raised_value.has_value(),
      Failure::invalid_frame_state);

    const auto layout = layout_type_id(type_id);
    internal_check(
      (layout.storage_size == 0) || (value_storage != nullptr),
      Failure::invalid_write);

    try
    {
      auto& raised = target_continuation->raised_value.emplace(
        RaisedValue{type_id, {}});
      raised.storage.resize(layout.storage_size);
      if (layout.storage_size != 0)
      {
        std::memcpy(
          raised.storage.data(), value_storage, layout.storage_size);
      }
    }
    catch (const std::bad_alloc&)
    {
      fail(Failure::out_of_memory);
    }

    if (
      (layout.value_type == ValueType::object) ||
      (layout.value_type == ValueType::array))
    {
      void* data_address = nullptr;
      std::memcpy(&data_address, value_storage, sizeof(data_address));
      auto* header = Value{layout.value_type, data_address}.header();
      auto* source = header->region();

      if ((source != nullptr) && source->is_frame_local())
      {
        auto* destination = frame_region(target);
        if (
          (source != destination) &&
          (source->frame_depth > destination->frame_depth) &&
          !drag_allocation(
            destination,
            header,
            {.root_reference = RootReference::retained}))
          raise_error(Error::bad_stack_escape);
      }
    }
    else if (layout.value_type == ValueType::reference)
    {
      internal_check(
        layout.storage_size == sizeof(Reference), Failure::invalid_value_state);
      Reference reference;
      std::memcpy(&reference, value_storage, sizeof(reference));
      vrt::reference::escape_to(reference, target);
    }

    unwind_frames(target);

    internal_check(
      (target_continuation != nullptr) &&
        (target_continuation->frame == target) && (thread.frame == target) &&
        target_continuation->raised_value.has_value(),
      Failure::invalid_frame_state);

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
