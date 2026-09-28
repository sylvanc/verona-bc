#include "array.h"

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

#include <cstring>
#include <limits>
#include <new>
#include <vector>
#include <vrt/array.h>

namespace vrt
{
  namespace
  {
    void check_range(uintptr_t size, uintptr_t offset, uintptr_t length)
    {
      if ((offset > size) || (length > (size - offset)))
        fail(Failure::invalid_array_state);
    }
  }

  Array::Array(
    Location location,
    uintptr_t type_id,
    ValueType value_type,
    uintptr_t size,
    uintptr_t stride,
    std::byte* allocation)
  : Header(location, type_id),
    size(size),
    stride(stride),
    element_value_type(value_type)
  {
    this->allocation = allocation;
  }

  Field Array::element() const
  {
    return Field{0, stride, content_type_id(), element_value_type};
  }

  Array* Array::create(
    std::byte* allocation,
    Location location,
    uintptr_t type_id,
    ValueType value_type,
    uintptr_t size,
    uintptr_t stride)
  {
    (void)size_of(size, stride);
    auto layout = layout_type_id(unarray(type_id));

    if (
      (allocation == nullptr) || !is_supported_storage_type(value_type) ||
      (layout_type_id(type_id).value_type != ValueType::array) ||
      (layout.value_type != value_type) || (layout.storage_size != stride) ||
      ((value_type != ValueType::none) && (size != 0) && (stride == 0)) ||
      (is_header_type(value_type) && (stride != sizeof(void*))) ||
      (location.is_region() &&
       (location.to_region()->destroying ||
        location.to_region()->is_finalizing())))
      fail(Failure::invalid_array_state);

    auto* array = ::new (allocation)
      Array{location, type_id, value_type, size, stride, allocation};
    std::memset(array->elements(), 0, size * stride);
    return array;
  }

  uintptr_t Array::content_type_id() const
  {
    return unarray(type_id);
  }

  size_t Array::size_of(uintptr_t size, uintptr_t stride)
  {
    if (
      (stride != 0) &&
      (size > ((std::numeric_limits<size_t>::max() - sizeof(Array)) / stride)))
      fail(Failure::invalid_array_state);

    return sizeof(Array) + (size * stride);
  }

  void* Array::load(uintptr_t index)
  {
    return const_cast<void*>(static_cast<const Array*>(this)->load(index));
  }

  const void* Array::load(uintptr_t index) const
  {
    if (index >= size)
      fail(Failure::invalid_array_state);

    return static_cast<const std::byte*>(elements()) + (stride * index);
  }

  bool Array::is_primitive() const
  {
    switch (element_value_type)
    {
      case ValueType::none:
      case ValueType::scalar:
      case ValueType::raw_pointer:
        return true;

      default:
        return false;
    }
  }

  void Array::bulk_copy(
    uintptr_t destination_offset,
    Array* source,
    uintptr_t source_offset,
    uintptr_t length)
  {
    if (length == 0)
      return;

    if (source == nullptr)
      fail(Failure::invalid_array_state);

    check_range(size, destination_offset, length);
    check_range(source->size, source_offset, length);
    if (
      (element_value_type != source->element_value_type) ||
      (content_type_id() != source->content_type_id()) ||
      (stride != source->stride))
      fail(Failure::invalid_array_state);

    auto* destination_data =
      static_cast<std::byte*>(elements()) + (stride * destination_offset);
    auto* source_data = static_cast<const std::byte*>(source->elements()) +
      (stride * source_offset);

    if (is_primitive())
    {
      std::memmove(destination_data, source_data, length * stride);
      return;
    }

    auto element_descriptor = element();
    if ((this == source) && (destination_offset > source_offset))
    {
      for (uintptr_t index = length; index > 0; index--)
      {
        writebarrier::copy(
          location(),
          load(destination_offset + index - 1),
          element_descriptor,
          source->load(source_offset + index - 1));
      }
      return;
    }

    for (uintptr_t index = 0; index < length; index++)
    {
      writebarrier::copy(
        location(),
        load(destination_offset + index),
        element_descriptor,
        source->load(source_offset + index));
    }
  }

  void
  Array::bulk_fill(uintptr_t offset, uintptr_t length, const void* fill_value)
  {
    if (length == 0)
      return;

    check_range(size, offset, length);
    if (fill_value == nullptr)
      fail(Failure::invalid_array_state);

    auto* destination = static_cast<std::byte*>(elements()) + (stride * offset);
    if (is_primitive())
    {
      if (stride == 1)
      {
        std::memset(
          destination, *static_cast<const uint8_t*>(fill_value), length);
        return;
      }

      std::vector<std::byte> value(stride);
      std::memcpy(value.data(), fill_value, stride);
      for (uintptr_t index = 0; index < length; index++)
        std::memcpy(destination + (stride * index), value.data(), stride);
      return;
    }

    void* data_address = nullptr;
    std::memcpy(&data_address, fill_value, sizeof(data_address));
    auto element_descriptor = element();
    for (uintptr_t index = 0; index < length; index++)
    {
      writebarrier::copy(
        location(),
        load(offset + index),
        element_descriptor,
        static_cast<const void*>(&data_address));
    }
  }

  int Array::bulk_compare(
    uintptr_t offset,
    const Array* other,
    uintptr_t other_offset,
    uintptr_t length) const
  {
    if (length == 0)
      return 0;

    if (other == nullptr)
      fail(Failure::invalid_array_state);

    check_range(size, offset, length);
    check_range(other->size, other_offset, length);
    if (
      !is_primitive() || !other->is_primitive() ||
      (element_value_type != other->element_value_type) ||
      (content_type_id() != other->content_type_id()) ||
      (stride != other->stride))
      fail(Failure::invalid_array_state);

    auto* left = static_cast<const std::byte*>(elements()) + (stride * offset);
    auto* right = static_cast<const std::byte*>(other->elements()) +
      (stride * other_offset);
    return std::memcmp(left, right, length * stride);
  }

  void Array::finalize()
  {
    if (location().is_immortal() || finalizing)
      return;

    finalizing = true;
    if (!is_header_type(element_value_type))
      return;

    auto element_descriptor = element();
    for (uintptr_t index = 0; index < size; index++)
      writebarrier::drop(location(), element_descriptor, load(index));
  }

  void Array::destroy_storage()
  {
    if (location().is_immortal())
      return;

    const bool release_allocation = !location().is_stack();
    auto* allocation = this->allocation;
    this->magic = 0;
    this->~Array();
    if (release_allocation)
      delete[] allocation;
  }
}

extern "C" VRT_EXPORT void* vrt_array_new(uintptr_t type_id, uintptr_t size)
{
  return vrt::current_frame_region()->array(type_id, size)->elements();
}

extern "C" VRT_EXPORT void* vrt_array_stack(uintptr_t type_id, uintptr_t size)
{
  auto& context = vrt::ThreadContext::get();
  auto* frame = context.thread.frame;
  internal_check(frame != nullptr, vrt::Failure::invalid_frame_state);
  return context.stack.array(frame->frame_id, type_id, size)->elements();
}

extern "C" VRT_EXPORT void*
vrt_array_heap(const void* region_locator, uintptr_t type_id, uintptr_t size)
{
  auto* region = vrt::Value{vrt::ValueType::object, region_locator}.region();
  internal_check(!region->destroying, vrt::Failure::invalid_region_state);
  return region->array(type_id, size)->elements();
}

extern "C" VRT_EXPORT void*
vrt_array_region(vrt::RegionType region_type, uintptr_t type_id, uintptr_t size)
{
  auto* region = vrt::Region::create(region_type);
  return region->array(type_id, size)->elements();
}

extern "C" VRT_EXPORT void vrt_array_retain(void* elements)
{
  vrt::ownership::retain_root(vrt::Value{vrt::ValueType::array, elements});
}

extern "C" VRT_EXPORT void vrt_array_release(void* elements)
{
  vrt::ownership::release_root(vrt::Value{vrt::ValueType::array, elements});
}

extern "C" VRT_EXPORT void vrt_array_freeze(void* elements)
{
  auto* header = vrt::Value{vrt::ValueType::array, elements}.header();
  if (!vrt::freeze(header))
    vrt::raise_error(vrt::Error::bad_freeze);
}

extern "C" VRT_EXPORT void vrt_array_escape(void* elements)
{
  vrt::ownership::escape(vrt::Value{vrt::ValueType::array, elements});
}

extern "C" VRT_EXPORT void vrt_array_validate_tailcall(void* elements)
{
  vrt::ownership::validate_tailcall(
    vrt::Value{vrt::ValueType::array, elements});
}

extern "C" VRT_EXPORT void vrt_array_copy(
  void* destination_elements,
  uintptr_t destination_offset,
  void* source_elements,
  uintptr_t source_offset,
  uintptr_t length)
{
  auto* destination = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, destination_elements}.header());
  auto* source = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, source_elements}.header());
  destination->bulk_copy(destination_offset, source, source_offset, length);
}

extern "C" VRT_EXPORT void vrt_array_fill(
  void* destination_elements,
  uintptr_t offset,
  uintptr_t length,
  const void* fill_value)
{
  auto* destination = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, destination_elements}.header());
  destination->bulk_fill(offset, length, fill_value);
}

extern "C" VRT_EXPORT int64_t vrt_array_compare(
  void* left_elements,
  uintptr_t left_offset,
  void* right_elements,
  uintptr_t right_offset,
  uintptr_t length)
{
  auto* left = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, left_elements}.header());
  auto* right = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, right_elements}.header());
  return static_cast<int64_t>(
    left->bulk_compare(left_offset, right, right_offset, length));
}
