// Coverage: immutable copy/drop ARC accounting, immutable children during
// graph drag, child-parent reuse during overwrite, ownership-transferring
// exchange, rejection of writes to immutable or immortal storage, and
// atomic rejection of conflicting region-parent stores and exchanges.
// Non-goals: return/raise dragging is covered by the frame and array fixtures;
// SCC reclamation is covered by the SCC fixture.

#include "writebarrier.h"

#include "error.h"
#include "frame.h"
#include "object.h"
#include "region.h"
#include "thread.h"
#include "vrt.h"

#include <cstddef>
#include <cstdint>
#include <vrt/error.h>
#include <vrt/program.h>

namespace
{
  constexpr uintptr_t node_class_id = 0x101;

  struct NodeFields
  {
    void* next;
  };

  const vrt::Field node_fields[] = {
    {offsetof(NodeFields, next),
     sizeof(NodeFields::next),
     node_class_id,
     vrt::ValueType::object}};

  const vrt::Class node_class{
    node_class_id,
    "Node",
    sizeof(NodeFields),
    alignof(NodeFields),
    1,
    node_fields,
    0,
    nullptr,
    nullptr};

  const vrt::TypeInfo types[] = {
    {node_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Program program{1, types, 0, nullptr};

  struct RejectedStore
  {
    vrt::Location location;
    void* target = nullptr;
    void* source;
  };

  struct RejectedExchange
  {
    vrt::Location location;
    void* target = nullptr;
    void* source;
    void* outgoing;
  };

  void rejected_store(void* raw_context)
  {
    auto* context = static_cast<RejectedStore*>(raw_context);
    vrt::writebarrier::copy(
      context->location, &context->target, node_fields[0], &context->source);
  }

  void rejected_exchange(void* raw_context)
  {
    auto* context = static_cast<RejectedExchange*>(raw_context);
    vrt::writebarrier::exchange(
      context->location,
      &context->target,
      node_fields[0],
      &context->source,
      &context->outgoing);
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt::init_thread();

  const vrt::Function root_function{1, "root", nullptr};
  auto* frame = vrt_frame_enter(&root_function);
  auto* frame_region = frame->region;

  auto* immutable_region = vrt::Region::create(vrt::RegionType::rc);
  auto* immutable = immutable_region->object(&node_class);
  if (!immutable_region->remove(immutable))
    return 1;
  immutable->set_location(vrt::Location::immutable());
  immutable->set_arc(1);
  if (immutable_region->stack_dec())
    return 2;

  auto* holder = frame_region->object(&node_class);
  auto* holder_fields = static_cast<NodeFields*>(holder->fields());
  auto* immutable_data = immutable->data();
  vrt::writebarrier::copy(
    holder->location(), &holder_fields->next, node_fields[0], &immutable_data);
  if ((holder_fields->next != immutable_data) || (immutable->get_arc() != 2))
    return 3;

  vrt::writebarrier::drop(
    holder->location(), node_fields[0], &holder_fields->next);
  if ((holder_fields->next != nullptr) || (immutable->get_arc() != 1))
    return 4;

  // Immutable children are terminal during mutable graph relocation.
  auto* immutable_destination_region = vrt::Region::create(vrt::RegionType::rc);
  auto* immutable_destination =
    immutable_destination_region->object(&node_class);
  auto* immutable_source = frame_region->object(&node_class);
  auto* immutable_source_fields =
    static_cast<NodeFields*>(immutable_source->fields());
  vrt::writebarrier::copy(
    immutable_source->location(),
    &immutable_source_fields->next,
    node_fields[0],
    &immutable_data);

  auto* immutable_source_data = immutable_source->data();
  auto* immutable_destination_fields =
    static_cast<NodeFields*>(immutable_destination->fields());
  vrt::writebarrier::copy(
    immutable_destination->location(),
    &immutable_destination_fields->next,
    node_fields[0],
    &immutable_source_data);
  if (
    (immutable_source->region() != immutable_destination_region) ||
    (immutable_destination_fields->next != immutable_source_data) ||
    (immutable->get_arc() != 2) ||
    (immutable_destination_region->stack_reference_count != 2))
    return 20;

  immutable_source->root_ref_dec();
  immutable_destination->root_ref_dec();
  if (immutable->get_arc() != 1)
    return 21;

  // Replacing the parent edge to a child region with a dragged graph that
  // still reaches that child must preserve the parent relationship.
  auto* reused_child_region = vrt::Region::create(vrt::RegionType::rc);
  auto* reused_child = reused_child_region->object(&node_class);
  auto* reuse_destination_region = vrt::Region::create(vrt::RegionType::rc);
  auto* reuse_destination = reuse_destination_region->object(&node_class);
  auto* reuse_destination_fields =
    static_cast<NodeFields*>(reuse_destination->fields());
  auto* reused_child_data = reused_child->data();
  vrt::writebarrier::init(
    reuse_destination->location(),
    &reuse_destination_fields->next,
    node_fields[0],
    &reused_child_data);

  auto* reuse_source = frame_region->object(&node_class);
  auto* reuse_source_fields = static_cast<NodeFields*>(reuse_source->fields());
  vrt::writebarrier::copy(
    reuse_source->location(),
    &reuse_source_fields->next,
    node_fields[0],
    &reused_child_data);

  auto* reuse_source_data = reuse_source->data();
  vrt::writebarrier::copy(
    reuse_destination->location(),
    &reuse_destination_fields->next,
    node_fields[0],
    &reuse_source_data);
  if (
    (reuse_source->region() != reuse_destination_region) ||
    (reuse_destination_fields->next != reuse_source_data) ||
    (reused_child_region->parent != reuse_destination_region) ||
    (reused_child_region->stack_reference_count != 0) ||
    (reused_child->reference_count != 1) ||
    (reuse_destination_region->stack_reference_count != 2))
    return 22;

  reuse_source->root_ref_dec();
  reuse_destination->root_ref_dec();

  holder->root_ref_dec();
  vrt_frame_leave();

  RejectedStore immutable_store{
    vrt::Location::immutable(), nullptr, immutable_data};
  vrt::ErrorInfo error{};
  if (vrt_try_invoke(rejected_store, &immutable_store, &error))
    return 5;
  if (
    (error.code != vrt::Error::bad_store_target) ||
    (immutable_store.target != nullptr) || (immutable->get_arc() != 1))
    return 6;

  RejectedStore immortal_store{
    vrt::Location::immortal(), nullptr, immutable_data};
  if (vrt_try_invoke(rejected_store, &immortal_store, &error))
    return 7;
  if (
    (error.code != vrt::Error::bad_store_target) ||
    (immortal_store.target != nullptr) || (immutable->get_arc() != 1))
    return 8;

  // Once a child region is owned through one parent, copying its entry point
  // into a different region would violate the single-parent invariant.
  auto* child_region = vrt::Region::create(vrt::RegionType::rc);
  auto* child = child_region->object(&node_class);
  auto* first_parent_region = vrt::Region::create(vrt::RegionType::rc);
  auto* first_parent = first_parent_region->object(&node_class);
  auto* child_data = child->data();
  auto* first_parent_fields = static_cast<NodeFields*>(first_parent->fields());
  vrt::writebarrier::init(
    first_parent->location(),
    &first_parent_fields->next,
    node_fields[0],
    &child_data);

  auto* second_parent_region = vrt::Region::create(vrt::RegionType::rc);
  auto* second_parent = second_parent_region->object(&node_class);
  RejectedStore conflicting_store{
    second_parent->location(),
    static_cast<NodeFields*>(second_parent->fields())->next,
    child_data};
  if (vrt_try_invoke(rejected_store, &conflicting_store, &error))
    return 9;
  if (
    (error.code != vrt::Error::bad_store) ||
    (conflicting_store.target != nullptr) ||
    (child_region->parent != first_parent_region))
    return 10;

  child->root_ref_inc();
  RejectedExchange conflicting_exchange{
    second_parent->location(), nullptr, child_data, immutable_data};
  if (vrt_try_invoke(rejected_exchange, &conflicting_exchange, &error))
    return 24;
  if (
    (error.code != vrt::Error::bad_store) ||
    (conflicting_exchange.target != nullptr) ||
    (conflicting_exchange.outgoing != immutable_data) ||
    (child_region->parent != first_parent_region))
    return 25;
  child->root_ref_dec();

  second_parent->root_ref_dec();
  first_parent->root_ref_dec();

  auto* exchange_destination_region = vrt::Region::create(vrt::RegionType::rc);
  auto* exchange_destination = exchange_destination_region->object(&node_class);
  auto* exchange_destination_fields =
    static_cast<NodeFields*>(exchange_destination->fields());
  auto* exchange_outgoing_region = vrt::Region::create(vrt::RegionType::rc);
  auto* exchange_outgoing = exchange_outgoing_region->object(&node_class);
  auto* exchange_outgoing_data = exchange_outgoing->data();
  vrt::writebarrier::init(
    exchange_destination->location(),
    &exchange_destination_fields->next,
    node_fields[0],
    &exchange_outgoing_data);

  auto* exchange_incoming_region = vrt::Region::create(vrt::RegionType::rc);
  auto* exchange_incoming = exchange_incoming_region->object(&node_class);
  auto* exchange_incoming_data = exchange_incoming->data();
  void* exchanged_data = nullptr;
  vrt::writebarrier::exchange(
    exchange_destination->location(),
    &exchange_destination_fields->next,
    node_fields[0],
    &exchange_incoming_data,
    &exchanged_data);
  if (
    (exchanged_data != exchange_outgoing_data) ||
    (exchange_destination_fields->next != exchange_incoming_data) ||
    exchange_outgoing_region->has_parent() ||
    (exchange_outgoing_region->stack_reference_count != 1) ||
    (exchange_incoming_region->parent != exchange_destination_region) ||
    (exchange_incoming_region->stack_reference_count != 0))
    return 23;

  exchange_outgoing->root_ref_dec();
  exchange_destination->root_ref_dec();

  immutable->root_ref_dec();
  vrt::deinit_thread();
  return vrt_thread_current() == nullptr ? 0 : 11;
}
