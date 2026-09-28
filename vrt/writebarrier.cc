#include "writebarrier.h"

#include "drag.h"
#include "error.h"
#include "failure.h"
#include "frame.h"
#include "header.h"
#include "object.h"
#include "ownership.h"
#include "region.h"
#include "thread_context.h"
#include "value.h"

#include <cstring>

namespace
{
  vrt::Value value(vrt::Header* header)
  {
    return vrt::Value{header->value_type(), header->data()};
  }

  vrt::Header* load_header(vrt::ValueType value_type, const void* source)
  {
    void* data_address = nullptr;
    std::memcpy(&data_address, source, sizeof(data_address));
    if (data_address == nullptr)
      return nullptr;

    return vrt::Header::from_data(value_type, data_address);
  }

  void store_header(void* target, vrt::Header* header)
  {
    auto* data_address = header->data();
    std::memcpy(target, &data_address, sizeof(data_address));
  }

  void clear_header(void* target)
  {
    void* data_address = nullptr;
    std::memcpy(target, &data_address, sizeof(data_address));
  }

  vrt::Region* frame_region_for_stack(vrt::Location location)
  {
    internal_check(location.is_stack(), vrt::Failure::invalid_write);

    auto* context = vrt::ThreadContext::try_get();
    internal_check(context != nullptr, vrt::Failure::invalid_write);

    auto* frame = context->thread.frame;
    while ((frame != nullptr) && (frame->frame_id != location))
      frame = frame->parent;

    internal_check(frame != nullptr, vrt::Failure::invalid_write);
    return vrt::frame_region(frame);
  }

  vrt::Region* store_region(vrt::Location location)
  {
    if (location.is_region())
      return location.to_region();

    if (location.is_stack())
      return frame_region_for_stack(location);

    return nullptr;
  }

  bool is_frame_storage(vrt::Location location)
  {
    return location.is_stack() ||
      (location.is_region() && location.to_region()->is_frame_local());
  }

  bool validate_stack_store(
    vrt::Location store_location, vrt::Location incoming_location)
  {
    if (!incoming_location.is_stack())
      return false;

    if (
      !store_location.is_stack() ||
      (store_location.stack_index() < incoming_location.stack_index()))
      vrt::raise_error(vrt::Error::bad_store);

    return true;
  }

  vrt::Region*
  replaced_child_region(vrt::Location store_location, vrt::Header* outgoing)
  {
    if ((outgoing == nullptr) || !store_location.is_region())
      return nullptr;

    const auto outgoing_location = outgoing->location();
    if (!outgoing_location.is_region())
      return nullptr;

    auto* destination = store_location.to_region();
    auto* outgoing_region = outgoing_location.to_region();
    if (
      (outgoing_region != destination) && outgoing_region->has_parent() &&
      (outgoing_region->parent == destination))
      return outgoing_region;

    return nullptr;
  }

  void
  validate_store(vrt::Location location, const void* target, const void* source)
  {
    internal_check(
      (target != nullptr) && (source != nullptr), vrt::Failure::invalid_write);

    if (location.is_immutable() || location.is_immortal())
      vrt::raise_error(vrt::Error::bad_store_target);

    internal_check(
      location.is_stack() || location.is_region(), vrt::Failure::invalid_write);

    if (location.is_region())
    {
      auto* region = location.to_region();
      internal_check(
        !region->destroying && !region->is_finalizing(),
        vrt::Failure::invalid_write);
    }
  }

  void drop_header(
    vrt::Location store_location,
    vrt::Header* outgoing,
    bool preserve_parent = false)
  {
    if (outgoing == nullptr)
      return;

    const auto outgoing_location = outgoing->location();
    if (outgoing_location.is_immortal() || outgoing_location.is_stack())
      return;

    if (outgoing_location.is_immutable())
    {
      vrt::ownership::release_field(value(outgoing));
      return;
    }

    auto* outgoing_region = outgoing->region();
    internal_check(outgoing_region != nullptr, vrt::Failure::invalid_write);

    if (outgoing_region->is_frame_local())
    {
      vrt::ownership::release_field(value(outgoing));
      return;
    }

    if (is_frame_storage(store_location))
    {
      outgoing_region->stack_dec();
      return;
    }

    if (store_location == outgoing_location)
    {
      vrt::ownership::release_field(value(outgoing));
      return;
    }

    if (preserve_parent)
    {
      internal_check(
        store_location.is_region() && outgoing_region->has_parent() &&
          (outgoing_region->parent == store_location.to_region()),
        vrt::Failure::invalid_write);
      vrt::ownership::release_field(value(outgoing));
      return;
    }

    if (outgoing_region->has_parent())
    {
      outgoing_region->clear_parent();
      return;
    }

    vrt::ownership::release_field(value(outgoing));
  }

  void transfer_outgoing(
    vrt::Location store_location,
    vrt::Header* outgoing,
    bool preserve_parent = false)
  {
    if (outgoing == nullptr)
      return;

    const auto outgoing_location = outgoing->location();
    if (
      outgoing_location.is_immortal() || outgoing_location.is_immutable() ||
      outgoing_location.is_stack())
      return;

    auto* outgoing_region = outgoing->region();
    internal_check(outgoing_region != nullptr, vrt::Failure::invalid_write);

    if (
      !outgoing_region->is_frame_local() &&
      !is_frame_storage(store_location))
      outgoing_region->stack_inc();

    if (store_location == outgoing_location)
      return;

    if (preserve_parent)
    {
      internal_check(
        store_location.is_region() && outgoing_region->has_parent() &&
          (outgoing_region->parent == store_location.to_region()),
        vrt::Failure::invalid_write);
      return;
    }

    if (outgoing_region->has_parent())
      outgoing_region->clear_parent();
  }
}

namespace vrt::writebarrier
{
  void init(
    Location store_location,
    void* target,
    const Field& field,
    const void* source)
  {
    validate_store(store_location, target, source);

    if (!is_header_type(field.value_type))
    {
      std::memcpy(target, source, field.size);
      return;
    }

    auto* incoming = load_header(field.value_type, source);
    internal_check(
      (incoming != nullptr) && !incoming->finalizing &&
        (incoming->get_type_id() == field.type_id),
      Failure::invalid_write);

    const auto incoming_location = incoming->location();
    if (incoming_location.is_immortal() || incoming_location.is_immutable())
    {
      store_header(target, incoming);
      return;
    }

    if (validate_stack_store(store_location, incoming_location))
    {
      store_header(target, incoming);
      return;
    }

    auto* incoming_region = incoming->region();
    internal_check(
      (incoming_region != nullptr) && !incoming_region->destroying,
      Failure::invalid_write);

    if (incoming_region->is_frame_local())
    {
      auto* destination = store_region(store_location);
      const bool must_drag =
        (store_location.is_stack() &&
         (store_location.stack_index() < incoming_region->frame_depth)) ||
        (store_location.is_region() &&
         (!destination->is_frame_local() ||
          ((destination != incoming_region) &&
           (destination->frame_depth < incoming_region->frame_depth))));

      if (must_drag && !drag_allocation(destination, incoming))
        raise_error(Error::bad_store);

      store_header(target, incoming);
      return;
    }

    // A reference stored in a frame-local region remains an external root of
    // the incoming heap region. Moving it out of the argument register and
    // into the field therefore leaves stack_reference_count unchanged.
    if (is_frame_storage(store_location))
    {
      store_header(target, incoming);
      return;
    }

    auto* destination = store_location.to_region();
    if (destination == incoming_region)
    {
      store_header(target, incoming);
      const bool incoming_region_alive = incoming_region->stack_dec();
      internal_check(incoming_region_alive, Failure::invalid_write);

      return;
    }

    if (
      incoming_region->has_parent() ||
      incoming_region->is_ancestor_of(destination))
      raise_error(Error::bad_alloc_target);

    // Establish ownership before consuming the stack reference. A child with
    // one stack reference is allowed to transition to zero only after it has
    // a parent.
    incoming_region->set_parent(destination, incoming);
    store_header(target, incoming);
    const bool incoming_region_alive = incoming_region->stack_dec();
    internal_check(incoming_region_alive, Failure::invalid_write);
  }

  void copy(
    Location store_location,
    void* target,
    const Field& field,
    const void* source)
  {
    validate_store(store_location, target, source);

    if (!is_header_type(field.value_type))
    {
      std::memmove(target, source, field.size);
      return;
    }

    auto* incoming = load_header(field.value_type, source);
    auto* outgoing = load_header(field.value_type, target);
    internal_check(incoming != nullptr, Failure::invalid_write);

    internal_check(
      !incoming->finalizing && (incoming->get_type_id() == field.type_id) &&
        ((incoming->region() != nullptr) ||
         incoming->location().is_stack() ||
         incoming->location().is_immutable() ||
         incoming->location().is_immortal()),
      Failure::invalid_write);

    if (incoming == outgoing)
      return;

    if (incoming->location().is_immortal())
    {
      store_header(target, incoming);
      drop_header(store_location, outgoing);
      return;
    }

    if (incoming->location().is_immutable())
    {
      vrt::ownership::retain_field(value(incoming));
      store_header(target, incoming);
      drop_header(store_location, outgoing);
      return;
    }

    if (validate_stack_store(store_location, incoming->location()))
    {
      store_header(target, incoming);
      drop_header(store_location, outgoing);
      return;
    }

    auto* incoming_region = incoming->region();
    internal_check(!incoming_region->destroying, Failure::invalid_write);

    if (incoming_region->is_frame_local())
    {
      auto* destination = store_region(store_location);
      const bool must_drag =
        (store_location.is_stack() &&
         (store_location.stack_index() < incoming_region->frame_depth)) ||
        (store_location.is_region() &&
         (!destination->is_frame_local() ||
          ((destination != incoming_region) &&
           (destination->frame_depth < incoming_region->frame_depth))));

      bool preserve_outgoing_parent = false;
      if (must_drag)
      {
        auto result = drag_allocation(
          destination,
          incoming,
          DragOptions{
            RootReference::retained,
            replaced_child_region(store_location, outgoing)});
        if (!result)
          raise_error(Error::bad_store);

        preserve_outgoing_parent = result->replaced_child_reused;
      }

      vrt::ownership::retain_field(value(incoming));
      store_header(target, incoming);
      drop_header(store_location, outgoing, preserve_outgoing_parent);
      return;
    }

    if (is_frame_storage(store_location))
    {
      vrt::ownership::retain_field(value(incoming));
      incoming_region->stack_inc();
      store_header(target, incoming);
      drop_header(store_location, outgoing);
      return;
    }

    auto* destination = store_location.to_region();
    if (destination == incoming_region)
    {
      vrt::ownership::retain_field(value(incoming));
      store_header(target, incoming);
      drop_header(store_location, outgoing);
      return;
    }

    if (
      incoming_region->has_parent() ||
      incoming_region->is_ancestor_of(destination))
      raise_error(Error::bad_store);

    vrt::ownership::retain_field(value(incoming));
    incoming_region->set_parent(destination, incoming);
    store_header(target, incoming);
    drop_header(store_location, outgoing);
  }

  void exchange(
    Location store_location,
    void* target,
    const Field& field,
    const void* incoming_storage,
    void* outgoing_storage)
  {
    validate_store(store_location, target, incoming_storage);
    internal_check(outgoing_storage != nullptr, Failure::invalid_write);

    if (!is_header_type(field.value_type))
    {
      std::memmove(outgoing_storage, target, field.size);
      std::memmove(target, incoming_storage, field.size);
      return;
    }

    auto* incoming = load_header(field.value_type, incoming_storage);
    auto* outgoing = load_header(field.value_type, target);
    internal_check(
      (incoming != nullptr) && !incoming->finalizing &&
        (incoming->get_type_id() == field.type_id),
      Failure::invalid_write);

    auto* outgoing_data = outgoing == nullptr ? nullptr : outgoing->data();
    std::memcpy(outgoing_storage, &outgoing_data, sizeof(outgoing_data));

    if (incoming == outgoing)
      return;

    const auto incoming_location = incoming->location();
    if (incoming_location.is_immortal() || incoming_location.is_immutable())
    {
      store_header(target, incoming);
      transfer_outgoing(store_location, outgoing);
      return;
    }

    if (validate_stack_store(store_location, incoming_location))
    {
      store_header(target, incoming);
      transfer_outgoing(store_location, outgoing);
      return;
    }

    auto* incoming_region = incoming->region();
    internal_check(
      (incoming_region != nullptr) && !incoming_region->destroying,
      Failure::invalid_write);

    if (incoming_region->is_frame_local())
    {
      auto* destination = store_region(store_location);
      const bool must_drag =
        (store_location.is_stack() &&
         (store_location.stack_index() < incoming_region->frame_depth)) ||
        (store_location.is_region() &&
         (!destination->is_frame_local() ||
          ((destination != incoming_region) &&
           (destination->frame_depth < incoming_region->frame_depth))));

      bool preserve_outgoing_parent = false;
      if (must_drag)
      {
        auto result = drag_allocation(
          destination,
          incoming,
          DragOptions{
            RootReference::transferred,
            replaced_child_region(store_location, outgoing)});
        if (!result)
          raise_error(Error::bad_store);

        preserve_outgoing_parent = result->replaced_child_reused;
      }

      store_header(target, incoming);
      transfer_outgoing(
        store_location, outgoing, preserve_outgoing_parent);
      return;
    }

    if (is_frame_storage(store_location))
    {
      store_header(target, incoming);
      transfer_outgoing(store_location, outgoing);
      return;
    }

    auto* destination = store_location.to_region();
    if (destination == incoming_region)
    {
      transfer_outgoing(store_location, outgoing);
      store_header(target, incoming);
      const bool incoming_region_alive = incoming_region->stack_dec();
      internal_check(incoming_region_alive, Failure::invalid_write);
      return;
    }

    const bool reuse_parent =
      (outgoing != nullptr) && (outgoing->region() == incoming_region) &&
      incoming_region->has_parent() &&
      (incoming_region->parent == destination);

    if (
      (!reuse_parent && incoming_region->has_parent()) ||
      incoming_region->is_ancestor_of(destination))
      raise_error(Error::bad_store);

    if (!reuse_parent)
      incoming_region->set_parent(destination, incoming);

    transfer_outgoing(store_location, outgoing, reuse_parent);
    store_header(target, incoming);
    const bool incoming_region_alive = incoming_region->stack_dec();
    internal_check(incoming_region_alive, Failure::invalid_write);
  }

  void drop(Location store_location, const Field& field, void* source)
  {
    internal_check(
      (source != nullptr) && is_header_type(field.value_type),
      Failure::invalid_write);

    auto* outgoing = load_header(field.value_type, source);
    clear_header(source);
    drop_header(store_location, outgoing);
  }
}
