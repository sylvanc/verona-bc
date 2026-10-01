// See README.md for Freeze coverage, runtime boundaries, and non-goals.

#include "freeze.h"

#include "array.h"
#include "frame.h"
#include "header.h"
#include "object.h"
#include "region.h"
#include "thread.h"
#include "vrt.h"
#include "writebarrier.h"

#include <cstddef>
#include <cstdint>
#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/program.h>

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t node_class_id = 0x101;
  constexpr uintptr_t singleton_class_id = 0x102;
  constexpr uintptr_t scalar_array_type_id = 0x200;
  constexpr uintptr_t node_array_type_id = 0x201;

  struct NodeFields
  {
    void* next;
    void* outgoing;
  };

  const vrt::Field node_fields[] = {
    {offsetof(NodeFields, next),
     sizeof(NodeFields::next),
     node_class_id,
     vrt::ValueType::object},
    {offsetof(NodeFields, outgoing),
     sizeof(NodeFields::outgoing),
     node_class_id,
     vrt::ValueType::object}};

  const vrt::Class node_class{
    node_class_id,
    "Node",
    sizeof(NodeFields),
    alignof(NodeFields),
    2,
    node_fields,
    0,
    nullptr,
    nullptr};

  alignas(vrt::Object) std::byte
    singleton_storage[vrt::Object::singleton_storage_size()]{};

  const vrt::Class singleton_class{
    singleton_class_id,
    "Singleton",
    0,
    1,
    0,
    nullptr,
    0,
    nullptr,
    singleton_storage + vrt::Object::singleton_data_offset()};

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint64_t), 0},
    {node_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {singleton_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {scalar_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_type_id},
    {node_array_type_id, vrt::ValueType::array, sizeof(void*), node_class_id}};
  const vrt::Singleton singletons[] = {{singleton_storage, &singleton_class}};
  const vrt::Program program{5, types, 1, singletons};

  const vrt::Function stack_function{2, "stack", nullptr};

  vrt::Object* new_node(vrt::Region* region)
  {
    return region->object(&node_class);
  }

  NodeFields* fields(vrt::Object* object)
  {
    return static_cast<NodeFields*>(object->fields());
  }

  void move_next(vrt::Object* source, vrt::Object* target)
  {
    auto* target_data = target->data();
    vrt::writebarrier::init(
      source->location(), &fields(source)->next, node_fields[0], &target_data);
  }

  void copy_next(vrt::Object* source, vrt::Object* target)
  {
    auto* target_data = target->data();
    vrt::writebarrier::copy(
      source->location(), &fields(source)->next, node_fields[0], &target_data);
  }

  void copy_outgoing(vrt::Object* source, vrt::Object* target)
  {
    auto* target_data = target->data();
    vrt::writebarrier::copy(
      source->location(),
      &fields(source)->outgoing,
      node_fields[1],
      &target_data);
  }

  void move_outgoing(vrt::Object* source, vrt::Object* target)
  {
    auto* target_data = target->data();
    vrt::writebarrier::init(
      source->location(),
      &fields(source)->outgoing,
      node_fields[1],
      &target_data);
  }

  vrt::Object* immutable_probe()
  {
    auto* region = vrt::Region::create(vrt::RegionType::rc);
    auto* probe = new_node(region);
    if (!region->remove(probe))
      return nullptr;

    probe->set_location(vrt::Location::immutable());
    probe->set_arc(1);
    if (region->stack_dec())
      return nullptr;

    return probe;
  }

  void freeze_object(void* data_address)
  {
    vrt_object_freeze(data_address);
  }

  void freeze_stack_object(void*)
  {
    auto* frame = vrt_frame_enter(&stack_function);
    auto* stack_object = new_node(frame->region);
    stack_object->set_location(frame->frame_id);
    vrt_object_freeze(stack_object->data());
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
  auto* probe = immutable_probe();
  if (probe == nullptr)
    return 1;

  // Immortal singletons are already deeply immutable, so Freeze is a no-op.
  auto* singleton = static_cast<vrt::Object*>(
    vrt::Header::from_data(vrt::ValueType::object, singleton_class.singleton));
  vrt_object_freeze(singleton_class.singleton);
  if (
    (singleton->location() != vrt::Location::immortal()) ||
    (singleton->reference_count != 1))
    return 27;

  // Acyclic objects become separate immutable SCCs. Re-freezing is a no-op,
  // and releasing the root recursively reclaims the child.
  auto* acyclic_region = vrt::Region::create(vrt::RegionType::rc);
  auto* acyclic_root = new_node(acyclic_region);
  auto* acyclic_child = new_node(acyclic_region);
  move_next(acyclic_root, acyclic_child);
  copy_outgoing(acyclic_child, probe);
  vrt_object_freeze(acyclic_root->data());
  if (
    (acyclic_root->location() != vrt::Location::immutable()) ||
    (acyclic_child->location() != vrt::Location::immutable()) ||
    (acyclic_root->get_arc() != 1) || (acyclic_child->get_arc() != 1) ||
    (probe->get_arc() != 2))
    return 2;
  vrt_object_freeze(acyclic_root->data());
  if ((acyclic_root->get_arc() != 1) || (acyclic_child->get_arc() != 1))
    return 3;
  acyclic_root->root_ref_dec();
  if (probe->get_arc() != 1)
    return 4;

  // A mutable two-object cycle publishes one representative with one external
  // ARC. Either member can carry the releasing register reference.
  auto* cycle_region = vrt::Region::create(vrt::RegionType::rc);
  auto* cycle_a = new_node(cycle_region);
  auto* cycle_b = new_node(cycle_region);
  move_next(cycle_a, cycle_b);
  copy_next(cycle_b, cycle_a);
  auto* cycle_non_representative =
    reinterpret_cast<uintptr_t>(cycle_a) <
      reinterpret_cast<uintptr_t>(cycle_b) ?
    cycle_a :
    cycle_b;
  copy_outgoing(cycle_non_representative, probe);
  vrt_object_freeze(cycle_a->data());
  auto* cycle_representative = cycle_a->representative();
  if (
    (cycle_b->representative() != cycle_representative) ||
    !cycle_non_representative->location().is_scc_ptr() ||
    (cycle_representative->location() != vrt::Location::immutable()) ||
    (cycle_representative->get_arc() != 1) || (probe->get_arc() != 2))
    return 5;
  vrt_object_freeze(cycle_non_representative->data());
  if (
    (cycle_non_representative->representative() != cycle_representative) ||
    (cycle_representative->get_arc() != 1) || (probe->get_arc() != 2))
    return 32;
  cycle_a->root_ref_dec();
  if (probe->get_arc() != 1)
    return 6;

  // An incoming edge from a surviving mutable object remains in the region
  // and accounts for one ARC without counting as a stack reference.
  auto* partial_region = vrt::Region::create(vrt::RegionType::rc);
  auto* partial_root = new_node(partial_region);
  auto* survivor = new_node(partial_region);
  copy_next(survivor, partial_root);
  copy_outgoing(partial_root, probe);
  vrt_object_freeze(partial_root->data());
  if (
    (partial_root->location() != vrt::Location::immutable()) ||
    (partial_root->get_arc() != 2) || (partial_region->header_count() != 1) ||
    !partial_region->contains(survivor) ||
    (partial_region->stack_reference_count != 1))
    return 7;
  partial_root->root_ref_dec();
  if ((partial_root->get_arc() != 1) || (probe->get_arc() != 2))
    return 8;
  survivor->root_ref_dec();
  if (probe->get_arc() != 1)
    return 9;

  // Reachability crosses sibling owned regions and clears both parent links
  // when their entry points are frozen.
  auto* left_region = vrt::Region::create(vrt::RegionType::rc);
  auto* left_child = new_node(left_region);
  copy_outgoing(left_child, probe);
  auto* right_region = vrt::Region::create(vrt::RegionType::rc);
  auto* right_child = new_node(right_region);
  copy_outgoing(right_child, probe);
  auto* parent_region = vrt::Region::create(vrt::RegionType::rc);
  auto* parent = new_node(parent_region);
  move_next(parent, left_child);
  move_outgoing(parent, right_child);
  vrt_object_freeze(parent->data());
  if (
    (parent->location() != vrt::Location::immutable()) ||
    (left_child->location() != vrt::Location::immutable()) ||
    (right_child->location() != vrt::Location::immutable()) ||
    (parent->get_arc() != 1) || (left_child->get_arc() != 1) ||
    (right_child->get_arc() != 1) || (probe->get_arc() != 3))
    return 10;
  parent->root_ref_dec();
  if (probe->get_arc() != 1)
    return 11;

  // Frame-local allocations freeze in place and leave the frame region empty.
  auto* local_root = new_node(frame_region);
  auto* local_child = new_node(frame_region);
  move_next(local_root, local_child);
  copy_outgoing(local_child, probe);
  vrt_object_freeze(local_root->data());
  if (
    (local_root->location() != vrt::Location::immutable()) ||
    (local_child->location() != vrt::Location::immutable()) ||
    (frame_region->header_count() != 0) || (probe->get_arc() != 2))
    return 12;
  local_root->root_ref_dec();
  if (probe->get_arc() != 1)
    return 13;

  // A frame-local root delegates reachable heap subregions to the regular
  // region Freeze path after publishing its local component.
  auto* local_heap_region = vrt::Region::create(vrt::RegionType::rc);
  auto* local_heap_child = new_node(local_heap_region);
  copy_outgoing(local_heap_child, probe);
  auto* local_heap_root = new_node(frame_region);
  move_next(local_heap_root, local_heap_child);
  vrt_object_freeze(local_heap_root->data());
  if (
    (local_heap_root->location() != vrt::Location::immutable()) ||
    (local_heap_child->location() != vrt::Location::immutable()) ||
    (local_heap_root->get_arc() != 1) ||
    (local_heap_child->get_arc() != 1) ||
    (frame_region->header_count() != 0) || (probe->get_arc() != 2))
    return 33;
  local_heap_root->root_ref_dec();
  if (probe->get_arc() != 1)
    return 34;

  // Primitive arrays use the same public Freeze and ARC lifetime path.
  auto* array_data =
    vrt_array_region(vrt::RegionType::rc, scalar_array_type_id, 4);
  auto* array = static_cast<vrt::Array*>(
    vrt::Header::from_data(vrt::ValueType::array, array_data));
  vrt_array_freeze(array_data);
  if (
    (array->location() != vrt::Location::immutable()) ||
    (array->get_arc() != 1))
    return 14;
  vrt_array_freeze(array_data);
  vrt_array_release(array_data);

  // Managed array elements participate in the same deep Freeze traversal.
  // Releasing the immutable array root recursively reclaims its child SCC.
  auto* managed_array_data =
    vrt_array_region(vrt::RegionType::rc, node_array_type_id, 1);
  auto* managed_array = static_cast<vrt::Array*>(
    vrt::Header::from_data(vrt::ValueType::array, managed_array_data));
  auto* managed_child = new_node(managed_array->region());
  auto* managed_child_data = managed_child->data();
  const vrt::Field managed_element{
    0, sizeof(void*), node_class_id, vrt::ValueType::object};
  vrt::writebarrier::init(
    managed_array->location(),
    managed_array->load(0),
    managed_element,
    &managed_child_data);
  copy_outgoing(managed_child, probe);
  vrt_array_freeze(managed_array_data);
  if (
    !managed_array->location().is_immutable() ||
    !managed_child->location().is_immutable() ||
    (managed_array->get_arc() != 1) || (managed_child->get_arc() != 1) ||
    (probe->get_arc() != 2))
    return 28;
  vrt_array_release(managed_array_data);
  if (probe->get_arc() != 1)
    return 29;

  // An edge to a member of an earlier immutable SCC is not internal to the
  // current Freeze and therefore does not suppress stack-RC adjustment.
  auto* prior_region = vrt::Region::create(vrt::RegionType::rc);
  auto* prior_a = new_node(prior_region);
  auto* prior_b = new_node(prior_region);
  move_next(prior_a, prior_b);
  copy_next(prior_b, prior_a);
  copy_outgoing(prior_a, probe);
  vrt_object_freeze(prior_a->data());
  auto* prior_representative = prior_a->representative();
  auto* prior_member = prior_a == prior_representative ? prior_b : prior_a;

  auto* later_region = vrt::Region::create(vrt::RegionType::rc);
  auto* later_root = new_node(later_region);
  auto* later_survivor = new_node(later_region);
  copy_outgoing(later_root, prior_member);
  vrt_object_freeze(later_root->data());
  if (
    (later_root->get_arc() != 1) || (later_region->header_count() != 1) ||
    (later_region->stack_reference_count != 1) ||
    !later_region->contains(later_survivor) ||
    (prior_representative->get_arc() != 2))
    return 15;
  later_root->root_ref_dec();
  if (prior_representative->get_arc() != 1)
    return 16;
  later_survivor->root_ref_dec();
  prior_a->root_ref_dec();
  if (probe->get_arc() != 1)
    return 17;

  // Owned regions are processed deepest-first so no descendant retains a
  // parent pointer to a region whose empty storage has already been released.
  auto* chain_leaf_region = vrt::Region::create(vrt::RegionType::rc);
  auto* chain_leaf = new_node(chain_leaf_region);
  copy_outgoing(chain_leaf, probe);
  auto* chain_middle_region = vrt::Region::create(vrt::RegionType::rc);
  auto* chain_middle = new_node(chain_middle_region);
  move_next(chain_middle, chain_leaf);
  auto* chain_root_region = vrt::Region::create(vrt::RegionType::rc);
  auto* chain_root = new_node(chain_root_region);
  move_next(chain_root, chain_middle);
  vrt_object_freeze(chain_root->data());
  if (
    !chain_root->location().is_immutable() ||
    !chain_middle->location().is_immutable() ||
    !chain_leaf->location().is_immutable() || (probe->get_arc() != 2))
    return 18;
  chain_root->root_ref_dec();
  if (probe->get_arc() != 1)
    return 19;

  // Unsupported reachable arenas reject the operation before publishing any
  // part of the mutable RC graph.
  auto* arena_child_region = vrt::Region::create(vrt::RegionType::arena);
  auto* arena_child = new_node(arena_child_region);
  auto* rejecting_region = vrt::Region::create(vrt::RegionType::rc);
  auto* rejecting_root = new_node(rejecting_region);
  auto* rejecting_sibling = new_node(rejecting_region);
  move_next(rejecting_root, arena_child);
  move_outgoing(rejecting_root, rejecting_sibling);
  const auto rejecting_stack_before = rejecting_region->stack_reference_count;
  const auto arena_stack_before = arena_child_region->stack_reference_count;
  if (vrt::freeze(rejecting_root))
    return 20;
  if (
    (rejecting_root->location() != vrt::Location(rejecting_region)) ||
    (rejecting_sibling->location() != vrt::Location(rejecting_region)) ||
    (arena_child->location() != vrt::Location(arena_child_region)) ||
    (rejecting_root->get_rc() != 1) || (rejecting_sibling->get_rc() != 1) ||
    (arena_child->get_rc() != 1) || (rejecting_region->header_count() != 2) ||
    (arena_child_region->header_count() != 1) ||
    (rejecting_region->stack_reference_count != rejecting_stack_before) ||
    (arena_child_region->stack_reference_count != arena_stack_before) ||
    !arena_child_region->has_parent() ||
    (arena_child_region->parent != rejecting_region))
    return 21;
  rejecting_root->root_ref_dec();

  // Freezing an interior object does not consume the parent's ownership edge
  // when the child region's entry point remains mutable.
  auto* owned_region = vrt::Region::create(vrt::RegionType::rc);
  auto* owned_entry = new_node(owned_region);
  auto* owned_interior = new_node(owned_region);
  move_next(owned_entry, owned_interior);
  owned_interior->root_ref_inc();
  copy_outgoing(owned_interior, probe);
  auto* owner_region = vrt::Region::create(vrt::RegionType::rc);
  auto* owner = new_node(owner_region);
  move_next(owner, owned_entry);
  vrt_object_freeze(owned_interior->data());
  if (
    !owned_interior->location().is_immutable() ||
    (owned_interior->get_arc() != 2) ||
    (owned_entry->location() != vrt::Location(owned_region)) ||
    (owned_region->stack_reference_count != 0) ||
    (owner_region->stack_reference_count != 1) || !owned_region->has_parent())
    return 22;
  owned_interior->root_ref_dec();
  owner->root_ref_dec();
  if (probe->get_arc() != 1)
    return 23;

  probe->root_ref_dec();
  vrt_frame_leave();

  // A stack Location cannot transition to immutable storage. The public API
  // reports BadFreeze and unwinds the frame that owns the manufactured stack
  // allocation.
  vrt::ErrorInfo error{};
  if (vrt_try_invoke(freeze_stack_object, nullptr, &error))
    return 30;
  if (
    (error.code != vrt::Error::bad_freeze) ||
    (vrt_thread_current_frame() != nullptr))
    return 31;

  // Arena objects cannot acquire per-object ARC state. Rejection leaves the
  // allocation mutable and owned by its arena.
  auto* arena = vrt::Region::create(vrt::RegionType::arena);
  auto* arena_root = new_node(arena);
  if (vrt_try_invoke(freeze_object, arena_root->data(), &error))
    return 24;
  if (
    (error.code != vrt::Error::bad_freeze) || (arena_root->region() != arena) ||
    !arena->contains(arena_root) || (arena->header_count() != 1) ||
    (arena->stack_reference_count != 1))
    return 25;
  arena_root->root_ref_dec();

  vrt::deinit_thread();
  return vrt_thread_current() == nullptr ? 0 : 26;
}
