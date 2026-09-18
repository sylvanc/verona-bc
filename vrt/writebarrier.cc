#include "writebarrier.h"

#include "error.h"
#include "failure.h"
#include "header.h"
#include "object.h"
#include "region.h"

#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
  vrt::Header* load_header(vrt::ValueType value_type, const void* source)
  {
    void* payload = nullptr;
    std::memcpy(&payload, source, sizeof(payload));
    if (payload == nullptr)
      return nullptr;

    return vrt::header_from_payload(value_type, payload);
  }

  void store_header(void* target, vrt::Header* header)
  {
    auto* payload = vrt::payload_from_header(header);
    std::memcpy(target, &payload, sizeof(payload));
  }

  void clear_header(void* target)
  {
    void* payload = nullptr;
    std::memcpy(target, &payload, sizeof(payload));
  }

  template<typename F>
  void trace_object(vrt::Object* object, F&& function)
  {
    auto* cls = object->cls;
    auto* payload = static_cast<std::byte*>(object->get_payload());
    for (uintptr_t index = 0; index < cls->field_count; index++)
    {
      const auto& field = cls->fields[index];
      if (!vrt::is_header_type(field.value_type))
        continue;

      auto* child = load_header(field.value_type, payload + field.offset);
      if (child != nullptr)
        function(child);
    }
  }

  template<typename F>
  void trace_header(vrt::Header* header, F&& function)
  {
    switch (header->value_type())
    {
      case vrt::ValueType::object:
        trace_object(
          static_cast<vrt::Object*>(header), std::forward<F>(function));
        return;

      default:
        vrt::fail(vrt::Failure::invalid_header_state);
    }
  }

  void drop_header(vrt::Region* store_region, vrt::Header* outgoing)
  {
    if (outgoing == nullptr)
      return;

    if (outgoing->location().is_immortal())
      return;

    auto* outgoing_region = outgoing->region();
    internal_check(outgoing_region != nullptr, vrt::Failure::invalid_write);

    if (outgoing_region->is_frame_local())
    {
      outgoing->field_dec();
      return;
    }

    if (store_region->is_frame_local())
    {
      outgoing_region->stack_dec();
      return;
    }

    if (store_region == outgoing_region)
    {
      outgoing->field_dec();
      return;
    }

    if (outgoing_region->has_parent())
    {
      outgoing_region->clear_parent();
      return;
    }

    outgoing->field_dec();
  }
}

namespace vrt::writebarrier
{
  void init(
    Region* store_region, void* target, const Field& field, const void* source)
  {
    internal_check(
      (store_region != nullptr) && !store_region->destroying &&
        !store_region->is_finalizing() && (target != nullptr) &&
        (source != nullptr),
      Failure::invalid_write);

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

    if (incoming->location().is_immortal())
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
      const bool must_drag = !store_region->is_frame_local() ||
        ((store_region != incoming_region) &&
         (store_region->frame_depth < incoming_region->frame_depth));

      if (must_drag && !drag(store_region, incoming))
        raise_error(Error::bad_store);

      store_header(target, incoming);
      return;
    }

    // A reference stored in a frame-local region remains an external root of
    // the incoming heap region. Moving it out of the argument register and
    // into the field therefore leaves stack_reference_count unchanged.
    if (store_region->is_frame_local())
    {
      store_header(target, incoming);
      return;
    }

    if (store_region == incoming_region)
    {
      store_header(target, incoming);
      const bool incoming_region_alive = incoming_region->stack_dec();
      internal_check(incoming_region_alive, Failure::invalid_write);

      return;
    }

    if (
      incoming_region->has_parent() ||
      incoming_region->is_ancestor_of(store_region))
      raise_error(Error::bad_alloc_target);

    // Establish ownership before consuming the stack reference. A child with
    // one stack reference is allowed to transition to zero only after it has
    // a parent.
    incoming_region->set_parent(store_region, incoming);
    store_header(target, incoming);
    const bool incoming_region_alive = incoming_region->stack_dec();
    internal_check(incoming_region_alive, Failure::invalid_write);
  }

  void copy(
    Region* store_region, void* target, const Field& field, const void* source)
  {
    internal_check(
      (store_region != nullptr) && !store_region->destroying &&
        !store_region->is_finalizing() && (target != nullptr) &&
        (source != nullptr),
      Failure::invalid_write);

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
         incoming->location().is_immortal()),
      Failure::invalid_write);

    if (incoming == outgoing)
      return;

    if (incoming->location().is_immortal())
    {
      store_header(target, incoming);
      drop_header(store_region, outgoing);
      return;
    }

    auto* incoming_region = incoming->region();
    internal_check(!incoming_region->destroying, Failure::invalid_write);

    if (incoming_region->is_frame_local())
    {
      const bool must_drag = !store_region->is_frame_local() ||
        ((store_region != incoming_region) &&
         (store_region->frame_depth < incoming_region->frame_depth));

      if (must_drag && !drag(store_region, incoming, false))
        raise_error(Error::bad_store);

      incoming->field_inc();
      store_header(target, incoming);
      drop_header(store_region, outgoing);
      return;
    }

    if (store_region->is_frame_local())
    {
      incoming->field_inc();
      incoming_region->stack_inc();
      store_header(target, incoming);
      drop_header(store_region, outgoing);
      return;
    }

    if (store_region == incoming_region)
    {
      incoming->field_inc();
      store_header(target, incoming);
      drop_header(store_region, outgoing);
      return;
    }

    if (
      incoming_region->has_parent() ||
      incoming_region->is_ancestor_of(store_region))
      raise_error(Error::bad_store);

    incoming->field_inc();
    incoming_region->set_parent(store_region, incoming);
    store_header(target, incoming);
    drop_header(store_region, outgoing);
  }

  void drop(Region* store_region, const Field& field, void* source)
  {
    internal_check(
      (store_region != nullptr) && (source != nullptr) &&
        is_header_type(field.value_type),
      Failure::invalid_write);

    auto* outgoing = load_header(field.value_type, source);
    clear_header(source);
    drop_header(store_region, outgoing);
  }

  bool drag(Region* destination, Header* root, bool root_reference_is_move)
  {
    if (
      (destination == nullptr) || destination->destroying ||
      destination->is_finalizing() || (root == nullptr) ||
      root->location().is_immortal() || root->finalizing ||
      (root->region() == nullptr) || !root->region()->is_frame_local())
      return false;

    if (root->region() == destination)
      return true;

    std::vector<Header*> worklist;
    std::unordered_map<Header*, uintptr_t> internal_references;
    std::unordered_map<Region*, Header*> child_regions;
    uintptr_t destination_stack_decrements = 0;
    worklist.push_back(root);

    while (!worklist.empty())
    {
      auto* header = worklist.back();
      worklist.pop_back();

      auto existing = internal_references.find(header);
      if (existing != internal_references.end())
      {
        if (existing->second == std::numeric_limits<uintptr_t>::max())
          return false;

        existing->second++;
        continue;
      }

      if (header->location().is_immortal())
        continue;

      auto* source_region = header->region();
      if (
        (source_region == nullptr) || source_region->destroying ||
        header->finalizing)
        return false;

      if (source_region == destination)
      {
        if (!destination->is_frame_local())
        {
          if (
            destination_stack_decrements ==
            std::numeric_limits<uintptr_t>::max())
            return false;

          destination_stack_decrements++;
        }

        continue;
      }

      if (
        destination->is_frame_local() && source_region->is_frame_local() &&
        (destination->frame_depth >= source_region->frame_depth))
        continue;

      if (!source_region->is_frame_local())
      {
        if (destination->is_frame_local())
          continue;

        if (
          source_region->has_parent() && (source_region->parent == destination))
          continue;

        if (
          source_region->has_parent() ||
          source_region->is_ancestor_of(destination) ||
          !child_regions.emplace(source_region, header).second)
          return false;

        continue;
      }

      internal_references.emplace(header, 1);
      trace_header(header, [&](Header* child) { worklist.push_back(child); });
    }

    if (!root_reference_is_move)
    {
      auto root_references = internal_references.find(root);
      if (
        (root_references == internal_references.end()) ||
        (root_references->second == 0))
        return false;

      root_references->second--;
    }

    for (const auto& [header, internal_count] : internal_references)
    {
      if (
        (header->reference_count < internal_count) ||
        (header->region() == nullptr) || !header->region()->contains(header))
        return false;
    }

    destination->stack_inc();

    for (const auto& [region, entry] : child_regions)
    {
      region->set_parent(destination, entry);
      const bool child_region_alive = region->stack_dec();
      internal_check(child_region_alive, Failure::invalid_write);
    }

    for (const auto& [header, internal_count] : internal_references)
    {
      auto* source_region = header->region();
      const auto external_count = header->reference_count - internal_count;
      destination->stack_inc(external_count);

      const bool removed = source_region->remove(header);
      internal_check(removed, Failure::invalid_write);

      header->set_location(Location(destination));
      destination->insert(header);
    }

    if (destination_stack_decrements != 0)
    {
      const bool destination_alive =
        destination->stack_dec(destination_stack_decrements);
      internal_check(destination_alive, Failure::invalid_write);
    }

    const bool destination_alive = destination->stack_dec();
    internal_check(destination_alive, Failure::invalid_write);

    return true;
  }
}
