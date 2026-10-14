#include "reference.h"

#include "array.h"
#include "error.h"
#include "failure.h"
#include "frame.h"
#include "object.h"
#include "ownership.h"
#include "program.h"
#include "thread_context.h"
#include "value.h"
#include "writebarrier.h"

#include <cstring>
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

  void validate(const vrt::Reference& reference)
  {
    if (kind(reference) == vrt::ReferenceKind::register_slot)
    {
      (void)active_register_frame(reference);
      return;
    }

    if ((reference.owner == nullptr) || (reference.target == nullptr))
      vrt::raise_error(vrt::Error::bad_reference_target);

    (void)owner_header(reference);
  }

  void retain_encoded(
    const vrt::TypeLayout& layout, const void* storage)
  {
    switch (layout.value_type)
    {
      case vrt::ValueType::none:
      case vrt::ValueType::scalar:
      case vrt::ValueType::raw_pointer:
        return;

      case vrt::ValueType::object:
      case vrt::ValueType::array:
      {
        void* data_address = nullptr;
        std::memcpy(&data_address, storage, sizeof(data_address));
        vrt::ownership::retain_root(
          vrt::Value{layout.value_type, data_address});
        return;
      }

      case vrt::ValueType::reference:
      {
        internal_check(
          layout.storage_size == sizeof(vrt::Reference),
          vrt::Failure::invalid_value_state);
        vrt::Reference reference;
        std::memcpy(&reference, storage, sizeof(reference));
        vrt_reference_retain(&reference);
        return;
      }

      case vrt::ValueType::cown:
      case vrt::ValueType::dynamic:
      case vrt::ValueType::aggregate:
        vrt::fail(vrt::Failure::invalid_value_state);
    }

    vrt::fail(vrt::Failure::invalid_value_state);
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

    ThreadContext::get().escape_to(owner_header(reference), target);
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

  const vrt::Value owner{vrt::ValueType::object, owned_object};
  auto* object = static_cast<vrt::Object*>(owner.header());
  const auto* field = object->cls->field(field_id);
  if (field == nullptr)
  {
    vrt::ownership::release_root(owner);
    vrt::raise_error(vrt::Error::bad_reference_target);
  }

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

  const vrt::Value owner{vrt::ValueType::array, owned_array};
  auto* array = static_cast<vrt::Array*>(owner.header());
  if (index >= array->get_size())
  {
    vrt::ownership::release_root(owner);
    vrt::raise_error(vrt::Error::bad_array_index);
  }

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
  {
    vrt::ownership::validate_tailcall(owner_value(*reference));
    return;
  }

  auto* owner = active_register_frame(*reference);
  if (owner == vrt::ThreadContext::get().thread.frame)
    vrt::raise_error(vrt::Error::bad_stack_escape);
}

extern "C" VRT_EXPORT void vrt_reference_load(
  const vrt::Reference* reference, void* output_storage)
{
  internal_check(reference != nullptr, vrt::Failure::invalid_value_state);
  validate(*reference);

  const auto layout = vrt::layout_type_id(reference->content_type_id);
  internal_check(
    (layout.storage_size == 0) || (output_storage != nullptr),
    vrt::Failure::invalid_write);

  if (layout.storage_size == 0)
    return;

  std::memmove(output_storage, reference->target, layout.storage_size);
  retain_encoded(layout, output_storage);
}

extern "C" VRT_EXPORT void vrt_reference_exchange(
  const vrt::Reference* reference,
  const void* owned_incoming_storage,
  void* outgoing_storage)
{
  internal_check(reference != nullptr, vrt::Failure::invalid_value_state);
  validate(*reference);

  const auto layout = vrt::layout_type_id(reference->content_type_id);
  internal_check(
    (layout.storage_size == 0) ||
      ((owned_incoming_storage != nullptr) && (outgoing_storage != nullptr)),
    vrt::Failure::invalid_write);

  if (layout.storage_size == 0)
    return;

  if (kind(*reference) == vrt::ReferenceKind::register_slot)
  {
    auto* owner = active_register_frame(*reference);
    switch (layout.value_type)
    {
      case vrt::ValueType::none:
        return;

      case vrt::ValueType::scalar:
      case vrt::ValueType::raw_pointer:
        std::memmove(
          outgoing_storage, reference->target, layout.storage_size);
        std::memmove(
          reference->target, owned_incoming_storage, layout.storage_size);
        return;

      case vrt::ValueType::object:
      case vrt::ValueType::array:
      {
        const vrt::Field field{
          0,
          layout.storage_size,
          reference->content_type_id,
          layout.value_type,
          0};
        vrt::writebarrier::exchange(
          owner->frame_id,
          reference->target,
          field,
          owned_incoming_storage,
          outgoing_storage);
        return;
      }

      case vrt::ValueType::reference:
      {
        internal_check(
          layout.storage_size == sizeof(vrt::Reference),
          vrt::Failure::invalid_value_state);
        vrt::Reference incoming;
        std::memcpy(&incoming, owned_incoming_storage, sizeof(incoming));
        vrt::reference::escape_to(incoming, owner);
        std::memmove(
          outgoing_storage, reference->target, layout.storage_size);
        std::memmove(
          reference->target, owned_incoming_storage, layout.storage_size);
        return;
      }

      case vrt::ValueType::cown:
      case vrt::ValueType::dynamic:
      case vrt::ValueType::aggregate:
        vrt::fail(vrt::Failure::invalid_value_state);
    }

    vrt::fail(vrt::Failure::invalid_value_state);
  }

  internal_check(
    vrt::is_supported_storage_type(layout.value_type),
    vrt::Failure::invalid_value_state);
  const vrt::Field field{
    0,
    layout.storage_size,
    reference->content_type_id,
    layout.value_type,
    0};
  vrt::writebarrier::exchange(
    owner_header(*reference)->location(),
    reference->target,
    field,
    owned_incoming_storage,
    outgoing_storage);
}
