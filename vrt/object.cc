#include "object.h"

#include "error.h"
#include "failure.h"
#include "frame.h"
#include "freeze.h"
#include "ownership.h"
#include "program.h"
#include "region.h"
#include "thread_context.h"
#include "value.h"
#include "writebarrier.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

namespace
{
  [[maybe_unused]] bool is_power_of_two(uintptr_t value)
  {
    return (value != 0) && ((value & (value - 1)) == 0);
  }

  void validate_class(const vrt::Class* cls)
  {
    internal_check(cls != nullptr, vrt::Failure::invalid_object_state);

    internal_check(
      vrt::layout_type_id(cls->id).value_type == vrt::ValueType::object,
      vrt::Failure::invalid_object_state);

    auto alignment = cls->data_alignment;
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
        vrt::is_supported_storage_type(field.value_type) &&
          (field.offset <= cls->data_size) &&
          (field.size <= (cls->data_size - field.offset)) &&
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
      (argc == cls->field_count) && ((argc == 0) || (packed_args != nullptr)),
      vrt::Failure::invalid_object_state);
  }

  bool is_singleton_class(const vrt::Class* cls)
  {
    return cls->singleton != nullptr;
  }

}

namespace vrt
{
  const Function* Class::method(uintptr_t method_id) const
  {
    uintptr_t first = 0;
    uintptr_t last = method_count;

    while (first < last)
    {
      const auto middle = first + ((last - first) / 2);
      const auto& candidate = methods[middle];

      if (candidate.id < method_id)
        first = middle + 1;
      else
        last = middle;
    }

    if ((first == method_count) || (methods[first].id != method_id))
      return nullptr;

    return methods[first].func;
  }

  const Function* Class::finalizer() const
  {
    return method(final_method_id);
  }

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
      std::max<uintptr_t>(cls->data_alignment, alignof(Object));
    const auto data_size = std::max<uintptr_t>(cls->data_size, uintptr_t{1});
    constexpr auto fixed_prefix = sizeof(Object);

    internal_check(
      ((alignment - 1) <=
       (std::numeric_limits<uintptr_t>::max() - fixed_prefix)) &&
        (data_size <= (std::numeric_limits<uintptr_t>::max() - fixed_prefix -
                       (alignment - 1))),
      Failure::invalid_object_state);

    return fixed_prefix + (alignment - 1) + data_size;
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
      std::max<uintptr_t>(cls->data_alignment, alignof(Object));
    constexpr auto fixed_prefix = sizeof(Object);

    const auto unaligned =
      reinterpret_cast<uintptr_t>(allocation + fixed_prefix);
    internal_check(
      unaligned <= (std::numeric_limits<uintptr_t>::max() - (alignment - 1)),
      Failure::invalid_object_state);

    const auto data_address = (unaligned + (alignment - 1)) & ~(alignment - 1);
    auto* fields = reinterpret_cast<std::byte*>(data_address);
    auto* object_storage = fields - sizeof(Object);
    auto* object =
      ::new (object_storage) Object{region, cls, allocation, immortal};
    std::memset(fields, 0, cls->data_size);

    return object;
  }

  Object& Object::init(uintptr_t argc, const void* packed_args)
  {
    auto* region = this->region();
    internal_check(
      !location().is_immortal() && (region != nullptr) && !finalizing,
      Failure::invalid_object_state);

    validate_arguments(cls, argc, packed_args);

    auto* target = static_cast<std::byte*>(fields());
    auto* source = static_cast<const std::byte*>(packed_args);
    for (uintptr_t index = 0; index < cls->field_count; index++)
    {
      const auto& field = cls->fields[index];
      writebarrier::init(
        location(), target + field.offset, field, source + field.offset);
    }

    return *this;
  }

  void Object::finalize()
  {
    if (location().is_immortal() || finalizing)
      return;

    finalizing = true;
    auto* fields = static_cast<std::byte*>(this->fields());
    const auto& cls = *this->cls;

    if (cls.finalizer_thunk != nullptr)
    {
      auto error =
        ThreadContext::get().run_cleanup(cls.finalizer_thunk, fields);
      if (error.code != Error::none)
      {
        std::fprintf(
          stderr,
          "runtime error during finalizer%s%s: %s\n",
          error.func == nullptr ? "" : " in ",
          error.func == nullptr ? "" : error.func->name,
          vrt_error_message(error.code));
        std::fflush(stderr);
      }
    }

    for (uintptr_t index = 0; index < cls.field_count; index++)
    {
      const auto& field = cls.fields[index];
      if (is_header_type(field.value_type))
        writebarrier::drop(location(), field, fields + field.offset);
    }
  }

  void Object::destroy_storage()
  {
    if (location().is_immortal())
      return;

    auto* allocation = this->allocation;
    magic = 0;
    this->~Object();
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
    object->fields() == cls->singleton, Failure::invalid_object_state);
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
    .fields();
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
  return region->object(cls)->init(argc, packed_args).fields();
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
  return region->object(cls)->init(argc, packed_args).fields();
}

extern "C" VRT_EXPORT uintptr_t vrt_object_class_id(const void* data_address)
{
  return vrt::Value{vrt::ValueType::object, data_address}
    .header()
    ->get_type_id();
}

extern "C" VRT_EXPORT const vrt::Function*
vrt_object_lookup(const void* data_address, uintptr_t method_id)
{
  const auto* object = static_cast<vrt::Object*>(
    vrt::Value{vrt::ValueType::object, data_address}.header());
  return object->cls->method(method_id);
}

extern "C" VRT_EXPORT void vrt_object_retain(void* data_address)
{
  vrt::ownership::retain_root(
    vrt::Value{vrt::ValueType::object, data_address});
}

extern "C" VRT_EXPORT void vrt_object_release(void* data_address)
{
  vrt::ownership::release_root(
    vrt::Value{vrt::ValueType::object, data_address});
}

extern "C" VRT_EXPORT void vrt_object_freeze(void* data_address)
{
  auto* header = vrt::Value{vrt::ValueType::object, data_address}.header();
  if (!vrt::freeze(header))
    vrt::raise_error(vrt::Error::bad_freeze);
}

extern "C" VRT_EXPORT void vrt_object_escape(void* data_address)
{
  vrt::ownership::escape(vrt::Value{vrt::ValueType::object, data_address});
}
