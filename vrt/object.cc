#include "object.h"

#include "error.h"
#include "failure.h"
#include "frame.h"
#include "program.h"
#include "region.h"
#include "value.h"
#include "writebarrier.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace
{
  [[maybe_unused]] bool is_power_of_two(uintptr_t value)
  {
    return (value != 0) && ((value & (value - 1)) == 0);
  }

  [[maybe_unused]] bool is_supported_value_type(vrt::ValueType value_type)
  {
    switch (value_type)
    {
      case vrt::ValueType::none:
      case vrt::ValueType::scalar:
      case vrt::ValueType::raw_pointer:
      case vrt::ValueType::object:
        return true;

      default:
        return false;
    }
  }

  void validate_class(const vrt::Class* cls)
  {
    internal_check(cls != nullptr, vrt::Failure::invalid_object_state);

    internal_check(
      vrt::layout_type_id(cls->id).value_type == vrt::ValueType::object,
      vrt::Failure::invalid_object_state);

    auto alignment = cls->payload_alignment;
    if (alignment == 0)
      alignment = 1;

    internal_check(
      is_power_of_two(alignment), vrt::Failure::invalid_object_state);

    internal_check(
      ((cls->field_count == 0) || (cls->fields != nullptr)) &&
        ((cls->method_count == 0) || (cls->methods != nullptr)),
      vrt::Failure::invalid_object_state);

    for (uintptr_t index = 0; index < cls->field_count; index++)
    {
      const auto& field = cls->fields[index];
      internal_check(
        is_supported_value_type(field.value_type) &&
          (field.offset <= cls->payload_size) &&
          (field.size <= (cls->payload_size - field.offset)) &&
          (!vrt::is_header_type(field.value_type) ||
           (field.size == sizeof(void*))),
        vrt::Failure::invalid_object_state);
    }

    for (uintptr_t index = 0; index < cls->method_count; index++)
    {
      const auto& method = cls->methods[index];
      internal_check(
        (method.func != nullptr) &&
          ((index == 0) || (cls->methods[index - 1].id < method.id)),
        vrt::Failure::invalid_object_state);
    }

    internal_check(
      (cls->field_count == 0) == (cls->singleton != nullptr),
      vrt::Failure::invalid_object_state);
  }

  void validate_arguments(
    const vrt::Class* cls, uintptr_t argc, const void* packed_args)
  {
    validate_class(cls);

    internal_check(
      (argc == cls->field_count) &&
        ((argc == 0) || (packed_args != nullptr)),
      vrt::Failure::invalid_object_state);
  }

  bool is_singleton_class(const vrt::Class* cls)
  {
    return cls->singleton != nullptr;
  }

}

namespace vrt
{
  Object::Object(
    Region* region, const Class* cls, std::byte* allocation, bool immortal)
  : Header(immortal ? Location::immortal() : Location(region), cls->id),
    cls(cls)
  {
    this->allocation = allocation;
  }

  size_t Object::size_of(const Class* cls)
  {
    validate_class(cls);

    const auto alignment =
      std::max<uintptr_t>(cls->payload_alignment, alignof(Object));
    const auto payload_size =
      std::max<uintptr_t>(cls->payload_size, uintptr_t{1});
    constexpr auto fixed_prefix = sizeof(Object);

    internal_check(
      ((alignment - 1) <=
       (std::numeric_limits<uintptr_t>::max() - fixed_prefix)) &&
        (payload_size <=
         (std::numeric_limits<uintptr_t>::max() - fixed_prefix -
          (alignment - 1))),
      Failure::invalid_object_state);

    return fixed_prefix + (alignment - 1) + payload_size;
  }

  Object* Object::create(
    std::byte* allocation, const Class* cls, Region* region, bool immortal)
  {
    (void)size_of(cls);

    internal_check(
      (allocation != nullptr) && (!immortal || (region == nullptr)) &&
        (immortal ||
         ((region != nullptr) && !region->destroying &&
          !region->is_finalizing())),
      Failure::invalid_object_state);

    const auto alignment =
      std::max<uintptr_t>(cls->payload_alignment, alignof(Object));
    constexpr auto fixed_prefix = sizeof(Object);

    const auto unaligned =
      reinterpret_cast<uintptr_t>(allocation + fixed_prefix);
    internal_check(
      unaligned <=
        (std::numeric_limits<uintptr_t>::max() - (alignment - 1)),
      Failure::invalid_object_state);

    const auto payload_address =
      (unaligned + (alignment - 1)) & ~(alignment - 1);
    auto* payload = reinterpret_cast<std::byte*>(payload_address);
    auto* object_storage = payload - sizeof(Object);
    auto* object =
      ::new (object_storage) Object{region, cls, allocation, immortal};
    std::memset(payload, 0, cls->payload_size);

    return object;
  }

  Object& Object::init(uintptr_t argc, const void* packed_args)
  {
    auto* region = this->region();
    internal_check(
      !location().is_immortal() && (region != nullptr) && !finalizing,
      Failure::invalid_object_state);

    validate_arguments(cls, argc, packed_args);

    auto* target = static_cast<std::byte*>(get_payload());
    auto* source = static_cast<const std::byte*>(packed_args);
    for (uintptr_t index = 0; index < cls->field_count; index++)
    {
      const auto& field = cls->fields[index];
      writebarrier::init(
        region, target + field.offset, field, source + field.offset);
    }

    return *this;
  }

  void finalize_object(Object* object)
  {
    if (
      (object == nullptr) || object->location().is_immortal() ||
      object->finalizing)
      return;

    object->finalizing = true;
    auto* cls = object->cls;
    auto* payload = static_cast<std::byte*>(object->get_payload());

    for (uintptr_t index = 0; index < cls->field_count; index++)
    {
      const auto& field = cls->fields[index];
      if (is_header_type(field.value_type))
        writebarrier::drop(object->region(), field, payload + field.offset);
    }
  }

  void destroy_object_storage(Object* object)
  {
    if ((object == nullptr) || object->location().is_immortal())
      return;

    auto* allocation = object->allocation;
    object->magic = 0;
    object->~Object();
    delete[] allocation;
  }
}

void vrt::init_singleton(void* storage, const Class* cls)
{
  validate_class(cls);
  internal_check(
    is_singleton_class(cls) && (storage != nullptr) &&
      ((reinterpret_cast<uintptr_t>(storage) % alignof(Object)) == 0),
    Failure::invalid_object_state);

  auto* object =
    Object::create(static_cast<std::byte*>(storage), cls, nullptr, true);
  internal_check(
    object->get_payload() == cls->singleton, Failure::invalid_object_state);
}

extern "C" VRT_EXPORT void*
vrt_object_new(const vrt::Class* cls, uintptr_t argc, const void* packed_args)
{
  validate_class(cls);

  if (is_singleton_class(cls))
    return cls->singleton;

  validate_arguments(cls, argc, packed_args);
  return vrt::current_frame_region()
    ->object(cls)
    ->init(argc, packed_args)
    .get_payload();
}

extern "C" VRT_EXPORT void* vrt_object_heap(
  const void* region_locator,
  const vrt::Class* cls,
  uintptr_t argc,
  const void* packed_args)
{
  auto* region = vrt::Value{vrt::ValueType::object, region_locator}.region();

  internal_check(!region->destroying, vrt::Failure::invalid_region_state);

  validate_class(cls);

  if (is_singleton_class(cls))
    return cls->singleton;

  validate_arguments(cls, argc, packed_args);
  return region->object(cls)->init(argc, packed_args).get_payload();
}

extern "C" VRT_EXPORT void* vrt_object_region(
  vrt::RegionType region_type,
  const vrt::Class* cls,
  uintptr_t argc,
  const void* packed_args)
{
  validate_class(cls);
  if (is_singleton_class(cls))
    vrt::raise_error(vrt::Error::bad_region_entry_point);

  validate_arguments(cls, argc, packed_args);
  auto* region = vrt::Region::create(region_type);
  return region->object(cls)->init(argc, packed_args).get_payload();
}

extern "C" VRT_EXPORT void vrt_object_retain(void* payload)
{
  vrt::Value{vrt::ValueType::object, payload}.reg_inc();
}

extern "C" VRT_EXPORT void vrt_object_release(void* payload)
{
  vrt::Value{vrt::ValueType::object, payload}.reg_dec();
}

extern "C" VRT_EXPORT void vrt_object_escape(void* payload)
{
  vrt::Value{vrt::ValueType::object, payload}.escape();
}

extern "C" VRT_EXPORT void vrt_object_prepare_raise(void* payload)
{
  vrt::Value{vrt::ValueType::object, payload}.prepare_raise();
}
