#include "reference.h"

#include "array.h"
#include "drag.h"
#include "error.h"
#include "failure.h"
#include "frame.h"
#include "object.h"
#include "ownership.h"
#include "program.h"
#include "region.h"
#include "thread_context.h"
#include "value.h"

namespace
{
  constexpr uintptr_t kind_mask = 0xff;

  vrt::ReferenceKind kind(const vrt::Reference& reference)
  {
    const auto raw = reference.kind_flags & kind_mask;
    if (raw > static_cast<uintptr_t>(vrt::ReferenceKind::array_element))
      vrt::fail(vrt::Failure::invalid_value_state);

    return static_cast<vrt::ReferenceKind>(raw);
  }

  vrt::Frame* active_register_frame(const vrt::Reference& reference)
  {
    auto* context = vrt::ThreadContext::try_get();
    internal_check(context != nullptr, vrt::Failure::invalid_thread_state);

    auto* owner = static_cast<vrt::Frame*>(reference.owner);
    auto* frame = context->thread.frame;
    while ((frame != nullptr) && (frame != owner))
      frame = frame->parent;

    if (
      (frame == nullptr) || (reference.target == nullptr) ||
      (reference.storage_epoch != owner->storage_epoch))
      vrt::raise_error(vrt::Error::bad_reference_target);

    return owner;
  }

  vrt::Value owner_value(const vrt::Reference& reference)
  {
    switch (kind(reference))
    {
      case vrt::ReferenceKind::object_field:
        return {vrt::ValueType::object, reference.owner};

      case vrt::ReferenceKind::array_element:
        return {vrt::ValueType::array, reference.owner};

      case vrt::ReferenceKind::register_slot:
        break;
    }

    vrt::fail(vrt::Failure::invalid_value_state);
  }

  vrt::Header* owner_header(const vrt::Reference& reference)
  {
    return owner_value(reference).header();
  }

  bool frame_survives(vrt::Frame* owner, vrt::Frame* target)
  {
    auto* frame = target;
    while ((frame != nullptr) && (frame != owner))
      frame = frame->parent;

    return frame == owner;
  }
}

namespace vrt::reference
{
  void escape_to(const Reference& reference, Frame* target)
  {
    internal_check(target != nullptr, Failure::invalid_frame_state);

    if (kind(reference) == ReferenceKind::register_slot)
    {
      auto* owner = active_register_frame(reference);
      if (!frame_survives(owner, target))
        raise_error(Error::bad_stack_escape);

      return;
    }

    auto* header = owner_header(reference);
    auto* source = header->region();
    if ((source == nullptr) || !source->is_frame_local())
      return;

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

extern "C" VRT_EXPORT void vrt_reference_from_register(
  vrt::Reference* out,
  vrt::Frame* frame,
  void* target,
  uintptr_t content_type_id)
{
  internal_check(
    (out != nullptr) && (frame != nullptr) && (target != nullptr),
    vrt::Failure::invalid_value_state);
  (void)vrt::layout_type_id(content_type_id);

  vrt::Reference candidate{
    static_cast<uintptr_t>(vrt::ReferenceKind::register_slot),
    frame,
    target,
    content_type_id,
    frame->storage_epoch};
  (void)active_register_frame(candidate);
  *out = candidate;
}

extern "C" VRT_EXPORT void vrt_reference_from_field(
  vrt::Reference* out, void* owned_object, uintptr_t field_id)
{
  internal_check(
    (out != nullptr) && (owned_object != nullptr),
    vrt::Failure::invalid_value_state);

  auto* object = static_cast<vrt::Object*>(
    vrt::Value{vrt::ValueType::object, owned_object}.header());
  const auto* field = object->cls->field(field_id);
  if (field == nullptr)
    vrt::raise_error(vrt::Error::bad_reference_target);

  (void)vrt::layout_type_id(field->type_id);
  *out = {
    static_cast<uintptr_t>(vrt::ReferenceKind::object_field),
    owned_object,
    static_cast<std::byte*>(owned_object) + field->offset,
    field->type_id,
    0};
}

extern "C" VRT_EXPORT void vrt_reference_from_array(
  vrt::Reference* out, void* owned_array, uintptr_t index)
{
  internal_check(
    (out != nullptr) && (owned_array != nullptr),
    vrt::Failure::invalid_value_state);

  auto* array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, owned_array}.header());
  if (index >= array->get_size())
    vrt::raise_error(vrt::Error::bad_array_index);

  *out = {
    static_cast<uintptr_t>(vrt::ReferenceKind::array_element),
    owned_array,
    array->load(index),
    array->content_type_id(),
    0};
}

extern "C" VRT_EXPORT void
vrt_reference_retain(const vrt::Reference* reference)
{
  internal_check(reference != nullptr, vrt::Failure::invalid_value_state);
  if (kind(*reference) == vrt::ReferenceKind::register_slot)
  {
    (void)active_register_frame(*reference);
    return;
  }

  vrt::ownership::retain_root(owner_value(*reference));
}

extern "C" VRT_EXPORT void
vrt_reference_release(const vrt::Reference* reference)
{
  internal_check(reference != nullptr, vrt::Failure::invalid_value_state);
  if (kind(*reference) == vrt::ReferenceKind::register_slot)
    return;

  vrt::ownership::release_root(owner_value(*reference));
}

extern "C" VRT_EXPORT void
vrt_reference_escape(const vrt::Reference* reference)
{
  internal_check(reference != nullptr, vrt::Failure::invalid_value_state);
  auto* current = vrt::ThreadContext::get().thread.frame;
  internal_check(current != nullptr, vrt::Failure::invalid_frame_state);

  if (kind(*reference) == vrt::ReferenceKind::register_slot)
  {
    auto* owner = active_register_frame(*reference);
    if (owner == current)
      vrt::raise_error(vrt::Error::bad_stack_escape);
    return;
  }

  vrt::ownership::escape(owner_value(*reference));
}

extern "C" VRT_EXPORT void
vrt_reference_validate_tailcall(const vrt::Reference* reference)
{
  internal_check(reference != nullptr, vrt::Failure::invalid_value_state);
  if (kind(*reference) != vrt::ReferenceKind::register_slot)
    return;

  auto* owner = active_register_frame(*reference);
  if (owner == vrt::ThreadContext::get().thread.frame)
    vrt::raise_error(vrt::Error::bad_stack_escape);
}
