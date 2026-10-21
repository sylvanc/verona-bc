#pragma once

#include "array.h"
#include "failure.h"
#include "header.h"
#include "object.h"
#include "region.h"
#include "region_arena.h"
#include "region_rc.h"

#include <cstring>
#include <utility>

namespace vrt
{
  template<typename F>
  void Object::trace_fn(F&& function)
  {
    const auto& class_descriptor = *cls;
    auto* object_fields = static_cast<std::byte*>(fields());

    for (uintptr_t index = 0; index < class_descriptor.field_count; index++)
    {
      const auto& field = class_descriptor.fields[index];
      if (!is_header_type(field.value_type))
        continue;

      void* data_address = nullptr;
      std::memcpy(
        &data_address, object_fields + field.offset, sizeof(data_address));
      if (data_address != nullptr)
        function(Header::from_data(field.value_type, data_address));
    }
  }

  template<typename F>
  void Header::trace_fn(F&& function)
  {
    switch (value_type())
    {
      case ValueType::object:
        static_cast<Object*>(this)->trace_fn(std::forward<F>(function));
        return;

      case ValueType::array:
        static_cast<Array*>(this)->trace_fn(std::forward<F>(function));
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

  template<typename F>
  void Region::trace_fn(F&& function) const
  {
    switch (type)
    {
      case RegionType::rc:
      case RegionType::arena:
        static_cast<const RegionRC*>(this)->trace_fn(std::forward<F>(function));
        return;
    }

    fail(Failure::invalid_region_state);
  }

  template<typename F>
  void Region::for_each_header(F&& function) const
  {
    switch (type)
    {
      case RegionType::rc:
      case RegionType::arena:
        static_cast<const RegionRC*>(this)->for_each_header(
          std::forward<F>(function));
        return;
    }

    fail(Failure::invalid_region_state);
  }

  template<typename F>
  void RegionRC::trace_fn(F&& function) const
  {
    for (auto* header : headers)
      header->trace_fn(function);
  }

  template<typename F>
  void RegionRC::for_each_header(F&& function) const
  {
    for (auto* header : headers)
      function(header);
  }
}