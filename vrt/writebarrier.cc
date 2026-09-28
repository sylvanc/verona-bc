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

    if (!outgoing_region->is_frame_local() && !is_frame_storage(store_location))
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

  enum class WriteKind
  {
    init,
    copy,
    exchange
  };

  enum class IncomingAction
  {
    direct,
    drag,
    frame_storage,
    same_region,
    set_parent,
    reuse_parent
  };

  struct WritePlan
  {
    WriteKind kind;
    vrt::Location store_location;
    vrt::Header* incoming;
    vrt::Header* outgoing;
    vrt::Location incoming_location;
    vrt::Region* incoming_region = nullptr;
    vrt::Region* destination = nullptr;
    vrt::Region* replaced_child = nullptr;
    IncomingAction action = IncomingAction::direct;
    void* outgoing_data = nullptr;
    bool same_value = false;

    static WritePlan prepare(
      WriteKind kind,
      vrt::Location store_location,
      vrt::Header* incoming,
      vrt::Header* outgoing)
    {
      WritePlan plan{
        kind, store_location, incoming, outgoing, incoming->location()};

      if (outgoing != nullptr)
        plan.outgoing_data = outgoing->data();

      if (incoming == outgoing)
      {
        plan.same_value = true;
        return plan;
      }

      if (
        plan.incoming_location.is_immortal() ||
        plan.incoming_location.is_immutable())
        return plan;

      if (plan.incoming_location.is_stack())
      {
        (void)validate_stack_store(store_location, plan.incoming_location);
        return plan;
      }

      plan.incoming_region = incoming->region();
      internal_check(
        (plan.incoming_region != nullptr) && !plan.incoming_region->destroying,
        vrt::Failure::invalid_write);

      if (plan.incoming_region->is_frame_local())
      {
        plan.destination = store_region(store_location);
        const bool must_drag = (store_location.is_stack() &&
                                (store_location.stack_index() <
                                 plan.incoming_region->frame_depth)) ||
          (store_location.is_region() &&
           (!plan.destination->is_frame_local() ||
            ((plan.destination != plan.incoming_region) &&
             (plan.destination->frame_depth <
              plan.incoming_region->frame_depth))));

        if (must_drag)
        {
          plan.action = IncomingAction::drag;
          plan.replaced_child = replaced_child_region(store_location, outgoing);
        }

        return plan;
      }

      if (is_frame_storage(store_location))
      {
        plan.action = IncomingAction::frame_storage;
        return plan;
      }

      plan.destination = store_location.to_region();
      if (plan.destination == plan.incoming_region)
      {
        plan.action = IncomingAction::same_region;
        return plan;
      }

      const bool reuse_parent = (kind == WriteKind::exchange) &&
        (outgoing != nullptr) && (outgoing->region() == plan.incoming_region) &&
        plan.incoming_region->has_parent() &&
        (plan.incoming_region->parent == plan.destination);

      if (
        (!reuse_parent && plan.incoming_region->has_parent()) ||
        plan.incoming_region->is_ancestor_of(plan.destination))
      {
        vrt::raise_error(
          kind == WriteKind::init ? vrt::Error::bad_alloc_target :
                                    vrt::Error::bad_store);
      }

      plan.action = reuse_parent ? IncomingAction::reuse_parent :
                                   IncomingAction::set_parent;
      return plan;
    }

    bool apply_drag()
    {
      if (action != IncomingAction::drag)
        return false;

      const auto root_reference = kind == WriteKind::copy ?
        vrt::RootReference::retained :
        vrt::RootReference::transferred;
      auto result = vrt::drag_allocation(
        destination,
        incoming,
        vrt::DragOptions{root_reference, replaced_child});
      if (!result)
        vrt::raise_error(vrt::Error::bad_store);

      return result->replaced_child_reused;
    }

    void commit_init(void* target)
    {
      if (same_value)
        return;

      (void)apply_drag();
      if (action == IncomingAction::set_parent)
        incoming_region->set_parent(destination, incoming);

      store_header(target, incoming);
      if (
        (action == IncomingAction::same_region) ||
        (action == IncomingAction::set_parent))
      {
        const bool incoming_region_alive = incoming_region->stack_dec();
        internal_check(incoming_region_alive, vrt::Failure::invalid_write);
      }
    }

    void commit_copy(void* target)
    {
      if (same_value)
        return;

      const bool preserve_outgoing_parent = apply_drag();
      if (incoming_location.is_immutable() || incoming_location.is_region())
        vrt::ownership::retain_field(value(incoming));

      if (action == IncomingAction::frame_storage)
        incoming_region->stack_inc();
      else if (action == IncomingAction::set_parent)
        incoming_region->set_parent(destination, incoming);

      store_header(target, incoming);
      drop_header(store_location, outgoing, preserve_outgoing_parent);
    }

    void commit_exchange(void* target, void* outgoing_storage)
    {
      if (!same_value)
      {
        const bool preserve_outgoing_parent =
          apply_drag() || (action == IncomingAction::reuse_parent);

        if (action == IncomingAction::set_parent)
          incoming_region->set_parent(destination, incoming);

        if (
          (action == IncomingAction::same_region) ||
          (action == IncomingAction::set_parent) ||
          (action == IncomingAction::reuse_parent))
        {
          transfer_outgoing(store_location, outgoing, preserve_outgoing_parent);
          store_header(target, incoming);
          const bool incoming_region_alive = incoming_region->stack_dec();
          internal_check(incoming_region_alive, vrt::Failure::invalid_write);
        }
        else
        {
          store_header(target, incoming);
          transfer_outgoing(store_location, outgoing, preserve_outgoing_parent);
        }
      }

      std::memcpy(outgoing_storage, &outgoing_data, sizeof(outgoing_data));
    }
  };
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

    auto plan =
      WritePlan::prepare(WriteKind::init, store_location, incoming, nullptr);
    plan.commit_init(target);
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
    internal_check(
      (incoming != nullptr) && !incoming->finalizing &&
        (incoming->get_type_id() == field.type_id),
      Failure::invalid_write);

    auto plan =
      WritePlan::prepare(WriteKind::copy, store_location, incoming, outgoing);
    plan.commit_copy(target);
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

    auto plan = WritePlan::prepare(
      WriteKind::exchange, store_location, incoming, outgoing);
    plan.commit_exchange(target, outgoing_storage);
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
