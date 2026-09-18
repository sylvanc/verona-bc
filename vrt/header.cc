#include "header.h"

#include "array.h"
#include "failure.h"
#include "object.h"
#include "program.h"
#include "region.h"
#include "value.h"

#include <limits>

namespace vrt
{
  namespace
  {
    void collect_header(Header* header)
    {
      if (
        (header == nullptr) || header->location().is_immortal() ||
        header->finalizing)
        return;

      auto* region = header->region();
      if (
        (region == nullptr) || region->destroying || region->is_finalizing() ||
        region->is_arena())
        return;

      // Dropping a child-region field can decrement this region's final stack
      // reference. Keep the region alive until this allocation has finished
      // finalizing and its storage is no longer reachable by the collector.
      region->stack_inc();

      if (!region->remove(header))
      {
        region->stack_dec();
        return;
      }

      header->finalize();
      header->destroy_storage();
      region->stack_dec();
    }
  }

  bool is_header_type(ValueType value_type)
  {
    return (value_type == ValueType::object) ||
      (value_type == ValueType::array);
  }

  ValueType Header::value_type() const
  {
    return layout_type_id(type_id).value_type;
  }

  void Header::reg_inc()
  {
    field_inc();
    if (loc.is_region())
      loc.to_region()->stack_inc();
  }

  void Header::reg_dec()
  {
    if (loc.is_region() && !loc.to_region()->stack_dec())
      return;

    field_dec();
  }

  void Header::field_inc()
  {
    if (loc.is_stack() || loc.is_immortal())
      return;

    auto* region = this->region();
    if (
      (region == nullptr) || region->destroying || region->is_finalizing() ||
      region->is_arena())
      return;

    internal_check(
      reference_count != std::numeric_limits<uintptr_t>::max(),
      Failure::invalid_header_state);

    reference_count++;
  }

  void Header::field_dec()
  {
    if (loc.is_stack() || loc.is_immortal())
      return;

    auto* region = this->region();
    if (
      (region == nullptr) || region->destroying || region->is_finalizing() ||
      region->is_arena())
      return;

    internal_check(reference_count != 0, Failure::invalid_header_state);

    reference_count--;
    if (reference_count == 0)
      collect_header(this);
  }

  Header* header_from_payload(ValueType value_type, const void* payload)
  {
    return Value{value_type, payload}.header();
  }

  void* payload_from_header(Header* header)
  {
    return const_cast<void*>(
      payload_from_header(static_cast<const Header*>(header)));
  }

  const void* payload_from_header(const Header* header)
  {
    internal_check(
      (header != nullptr) && (header->magic == Header::magic_value),
      Failure::invalid_header_state);

    switch (header->value_type())
    {
      case ValueType::object:
        return static_cast<const Object*>(header)->get_payload();

      case ValueType::array:
        return static_cast<const Array*>(header)->get_payload();

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void Header::finalize()
  {
    switch (value_type())
    {
      case ValueType::object:
        static_cast<Object*>(this)->finalize();
        return;

      case ValueType::array:
        static_cast<Array*>(this)->finalize();
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void Header::destroy_storage()
  {
    switch (value_type())
    {
      case ValueType::object:
        static_cast<Object*>(this)->destroy_storage();
        return;

      case ValueType::array:
        static_cast<Array*>(this)->destroy_storage();
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

}
