#include "header.h"

#include "error.h"
#include "failure.h"
#include "frame.h"
#include "thread_context.h"
#include "object.h"
#include "program.h"
#include "region.h"
#include "value.h"
#include "writebarrier.h"

#include <limits>

namespace vrt
{
  namespace
  {
    void escape_to_frame(Header* header, Frame* destination_frame)
    {
      if (header->location().is_immortal())
        return;

      auto* source = header->region();
      if ((source == nullptr) || !source->is_frame_local())
        return;

      auto* destination = frame_region(destination_frame);
      if (
        (source == destination) ||
        (source->frame_depth <= destination->frame_depth))
        return;

      if (!writebarrier::drag(destination, header, false))
        raise_error(Error::bad_stack_escape);
    }

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

      finalize_header(header);
      destroy_header_storage(header);
      region->stack_dec();
    }
  }

  bool is_header_type(ValueType value_type)
  {
    return value_type == ValueType::object;
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

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void finalize_header(Header* header)
  {
    internal_check(header != nullptr, Failure::invalid_header_state);

    switch (header->value_type())
    {
      case ValueType::object:
        finalize_object(static_cast<Object*>(header));
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void destroy_header_storage(Header* header)
  {
    internal_check(header != nullptr, Failure::invalid_header_state);

    switch (header->value_type())
    {
      case ValueType::object:
        destroy_object_storage(static_cast<Object*>(header));
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void escape_header(Header* header)
  {
    internal_check(header != nullptr, Failure::invalid_header_state);

    if (header->location().is_immortal())
      return;

    auto* context = ThreadContext::try_get();
    internal_check(
      (context != nullptr) && (context->thread.frame != nullptr),
      Failure::invalid_header_state);

    auto* frame = context->thread.frame;
    auto* source = header->region();
    if ((source == nullptr) || !source->is_frame_local())
      return;

    if (frame->region != source)
      return;

    if (frame->parent != nullptr)
    {
      escape_to_frame(header, frame->parent);
      return;
    }

    auto* destination = Region::create(RegionType::rc);
    if (!writebarrier::drag(destination, header, false))
    {
      destroy_region(destination);
      raise_error(Error::bad_stack_escape);
    }
  }

  void prepare_raise_header(Header* header)
  {
    internal_check(header != nullptr, Failure::invalid_header_state);

    if ((header->region() == nullptr) || !header->region()->is_frame_local())
      return;

    auto* context = ThreadContext::try_get();
    internal_check(
      (context != nullptr) && (context->thread.frame != nullptr),
      Failure::invalid_header_state);

    auto* current = context->thread.frame;
    auto raise_target = current->raise_target;
    if (!raise_target.is_stack() || (raise_target >= current->frame_id))
      raise_error(Error::bad_raise_target);

    auto* target = current->parent;
    while ((target != nullptr) && (target->frame_id != raise_target))
      target = target->parent;

    if (target == nullptr)
      raise_error(Error::bad_raise_target);

    escape_to_frame(header, target);
  }
}
