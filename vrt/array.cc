#include "array.h"

#include "failure.h"
#include "program.h"
#include "region.h"
#include "writebarrier.h"

#include <cstring>
#include <limits>
#include <new>
#include <vrt/array.h>

namespace vrt
{
  namespace
  {
    bool is_supported_value_type(ValueType value_type)
    {
      switch (value_type)
      {
        case ValueType::none:
        case ValueType::scalar:
        case ValueType::raw_pointer:
        case ValueType::object:
        case ValueType::array:
          return true;

        default:
          return false;
      }
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
      (allocation == nullptr) || !is_supported_value_type(value_type) ||
      (layout_type_id(type_id).value_type != ValueType::array) ||
      (layout.value_type != value_type) || (layout.storage_size != stride) ||
      ((value_type != ValueType::none) && (size != 0) && (stride == 0)) ||
      (is_header_type(value_type) && (stride != sizeof(void*))) ||
      (location.is_region() &&
       (location.to_region()->destroying ||
        location.to_region()->is_finalizing())))
      fail(Failure::invalid_array_state);

    auto* array = ::new (allocation) Array{
      location,
      type_id,
      value_type,
      size,
      stride,
      allocation};
    std::memset(array->get_pointer(), 0, size * stride);
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

    return static_cast<const std::byte*>(get_pointer()) + (stride * index);
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
      writebarrier::drop(region(), element_descriptor, load(index));
  }

  void Array::destroy_storage()
  {
    if (location().is_immortal())
      return;

    auto* allocation = this->allocation;
    this->magic = 0;
    this->~Array();
    delete[] allocation;
  }
}

extern "C" VRT_EXPORT void*
vrt_array_new(uintptr_t type_id, uintptr_t size)
{
  return vrt::current_frame_region()->array(type_id, size)->get_payload();
}

extern "C" VRT_EXPORT void* vrt_array_heap(
  const void* region_locator, uintptr_t type_id, uintptr_t size)
{
  auto* region =
    vrt::Value{vrt::ValueType::object, region_locator}.region();
  internal_check(!region->destroying, vrt::Failure::invalid_region_state);
  return region->array(type_id, size)->get_payload();
}

extern "C" VRT_EXPORT void* vrt_array_region(
  vrt::RegionType region_type, uintptr_t type_id, uintptr_t size)
{
  auto* region = vrt::Region::create(region_type);
  return region->array(type_id, size)->get_payload();
}

extern "C" VRT_EXPORT void vrt_array_retain(void* payload)
{
  vrt::Value{vrt::ValueType::array, payload}.reg_inc();
}

extern "C" VRT_EXPORT void vrt_array_release(void* payload)
{
  vrt::Value{vrt::ValueType::array, payload}.reg_dec();
}

extern "C" VRT_EXPORT void vrt_array_escape(void* payload)
{
  vrt::Value{vrt::ValueType::array, payload}.escape();
}
