#include "region.h"

#include "array.h"
#include "frame.h"
#include "object.h"
#include "region_arena.h"
#include "region_rc.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vrt/array.h>
#include <vrt/program.h>
#include <vrt/thread.h>

static_assert(std::is_abstract_v<vrt::Region>);
static_assert(std::is_base_of_v<vrt::Region, vrt::RegionRC>);
static_assert(std::is_base_of_v<vrt::RegionRC, vrt::RegionArena>);

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t holder_class_id = 0x102;
  constexpr uintptr_t scalar_array_type_id = 0x201;

  struct ValuePayload
  {
    uint64_t value;
  };

  struct HolderPayload
  {
    void* value;
    uint32_t tag;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValuePayload, value),
     sizeof(ValuePayload::value),
     0,
     vrt::ValueType::scalar}};

  const vrt::Field holder_fields[] = {
    {offsetof(HolderPayload, value),
     sizeof(HolderPayload::value),
     value_class_id,
     vrt::ValueType::object},
    {offsetof(HolderPayload, tag),
     sizeof(HolderPayload::tag),
     0,
     vrt::ValueType::scalar}};

  vrt::Class value_class{
    value_class_id,
    "Value",
    sizeof(ValuePayload),
    alignof(ValuePayload),
    1,
    value_fields,
    0,
    nullptr,
    nullptr};

  vrt::Class holder_class{
    holder_class_id,
    "Holder",
    sizeof(HolderPayload),
    alignof(HolderPayload),
    2,
    holder_fields,
    0,
    nullptr,
    nullptr};

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint32_t), 0},
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {holder_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {scalar_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_type_id}};
  const vrt::Program program{4, types, 0, nullptr};

  vrt::Object* object_from_payload(void* payload)
  {
    return static_cast<vrt::Object*>(
      vrt::header_from_payload(vrt::ValueType::object, payload));
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);

  if (
    (value_class.id != value_class_id) ||
    (holder_class.id != holder_class_id) || (holder_class.field_count != 2) ||
    (holder_fields[0].type_id != value_class_id) ||
    (holder_fields[0].value_type != vrt::ValueType::object))
    return 1;

  const vrt::Func root_function{1, "root", nullptr};
  const vrt::Func tail_function{2, "tail", nullptr};
  vrt_thread_init();
  auto* root_frame = vrt_frame_enter(&root_function);
  if (
    (root_frame == nullptr) || (root_frame->region == nullptr) ||
    !root_frame->region->is_frame_local() ||
    (root_frame->region->frame_depth != 1) ||
    (root_frame->region->type != vrt::RegionType::rc) ||
    (dynamic_cast<vrt::RegionArena*>(root_frame->region) != nullptr) ||
    (dynamic_cast<vrt::RegionRC*>(root_frame->region) == nullptr) ||
    (root_frame->region->header_count() != 0))
    return 2;

  auto* frame_region = root_frame->region;

  auto* frame_array_payload = vrt_array_new(scalar_array_type_id, 2);
  auto* frame_array = static_cast<vrt::Array*>(
    vrt::header_from_payload(vrt::ValueType::array, frame_array_payload));
  if (
    (frame_array->region() != frame_region) ||
    !frame_region->contains(frame_array) ||
    (frame_region->header_count() != 1))
    return 18;

  vrt_array_release(frame_array_payload);
  if (frame_region->header_count() != 0)
    return 19;

  auto* rc_array_payload =
    vrt_array_region(vrt::RegionType::rc, scalar_array_type_id, 2);
  auto* rc_array = static_cast<vrt::Array*>(
    vrt::header_from_payload(vrt::ValueType::array, rc_array_payload));
  auto* rc_array_region = rc_array->region();
  if (
    (rc_array_region == nullptr) || rc_array_region->is_frame_local() ||
    (rc_array_region->type != vrt::RegionType::rc) ||
    (dynamic_cast<vrt::RegionRC*>(rc_array_region) == nullptr) ||
    (dynamic_cast<vrt::RegionArena*>(rc_array_region) != nullptr) ||
    (rc_array_region->stack_reference_count != 1) ||
    (rc_array_region->header_count() != 1) ||
    !rc_array_region->contains(rc_array))
    return 20;
  vrt_array_release(rc_array_payload);

  auto* arena_array_payload =
    vrt_array_region(vrt::RegionType::arena, scalar_array_type_id, 2);
  auto* arena_array = static_cast<vrt::Array*>(
    vrt::header_from_payload(vrt::ValueType::array, arena_array_payload));
  auto* arena_array_region = arena_array->region();
  if (
    (arena_array_region == nullptr) || arena_array_region->is_frame_local() ||
    (arena_array_region->type != vrt::RegionType::arena) ||
    (dynamic_cast<vrt::RegionArena*>(arena_array_region) == nullptr) ||
    (arena_array_region->stack_reference_count != 1) ||
    (arena_array_region->header_count() != 1) ||
    !arena_array_region->contains(arena_array))
    return 21;
  vrt_array_release(arena_array_payload);

  // Region initialization drags a frame-local object graph into the new RC
  // region. The argument ownership becomes the Holder field ownership.
  ValuePayload dragged_args{84};
  auto* dragged_value = vrt_object_new(&value_class, 1, &dragged_args);
  auto* dragged_object = object_from_payload(dragged_value);
  HolderPayload dragged_holder_args{dragged_value, 7};
  auto* dragged_holder = vrt_object_region(
    vrt::RegionType::rc, &holder_class, 2, &dragged_holder_args);
  auto* dragged_holder_object = object_from_payload(dragged_holder);
  auto* dragged_region = dragged_holder_object->region();
  auto* dragged_payload = static_cast<HolderPayload*>(dragged_holder);
  if (
    (dragged_payload->value != dragged_value) || (dragged_payload->tag != 7) ||
    (dragged_object->region() != dragged_region) ||
    frame_region->contains(dragged_object) ||
    (dragged_region->type != vrt::RegionType::rc) ||
    dragged_region->is_frame_local() || dragged_region->has_parent() ||
    (dragged_region->stack_reference_count != 1) ||
    (dragged_region->header_count() != 2) ||
    !dragged_region->contains(dragged_object) ||
    !dragged_region->contains(dragged_holder_object))
    return 3;

  vrt_object_release(dragged_holder);
  if (frame_region->header_count() != 0)
    return 4;

  // Heap reuses the locator's region. Releasing an ordinary RC object
  // collects it while the region and its entry object remain live.
  ValuePayload rc_root_args{1};
  auto* rc_root =
    vrt_object_region(vrt::RegionType::rc, &value_class, 1, &rc_root_args);
  auto* rc_root_object = object_from_payload(rc_root);
  auto* rc_region = rc_root_object->region();
  ValuePayload rc_heap_args{2};
  auto* rc_heap = vrt_object_heap(rc_root, &value_class, 1, &rc_heap_args);
  auto* rc_heap_object = object_from_payload(rc_heap);
  auto* rc_heap_array_payload =
    vrt_array_heap(rc_root, scalar_array_type_id, 2);
  auto* rc_heap_array = static_cast<vrt::Array*>(
    vrt::header_from_payload(vrt::ValueType::array, rc_heap_array_payload));
  if (
    (rc_heap_object->region() != rc_region) ||
    (rc_heap_array->region() != rc_region) ||
    (static_cast<ValuePayload*>(rc_heap)->value != 2) ||
    (rc_region->stack_reference_count != 3) ||
    (rc_region->header_count() != 3) || !rc_region->contains(rc_heap_array))
    return 5;

  vrt_array_release(rc_heap_array_payload);
  if (
    (rc_region->stack_reference_count != 2) ||
    (rc_region->header_count() != 2) || rc_region->contains(rc_heap_array))
    return 22;

  vrt_object_release(rc_heap);
  if (
    (rc_region->stack_reference_count != 1) ||
    (rc_region->header_count() != 1) || !rc_region->contains(rc_root_object))
    return 6;

  vrt_object_release(rc_root);

  // Arena releases consume stack ownership without collecting individual
  // objects; releasing the final region root tears the whole arena down.
  ValuePayload arena_root_args{3};
  auto* arena_root = vrt_object_region(
    vrt::RegionType::arena, &value_class, 1, &arena_root_args);
  auto* arena_root_object = object_from_payload(arena_root);
  auto* arena_region = arena_root_object->region();
  ValuePayload arena_heap_args{4};
  auto* arena_heap =
    vrt_object_heap(arena_root, &value_class, 1, &arena_heap_args);
  auto* arena_heap_object = object_from_payload(arena_heap);
  auto* arena_heap_array_payload =
    vrt_array_heap(arena_root, scalar_array_type_id, 2);
  auto* arena_heap_array = static_cast<vrt::Array*>(
    vrt::header_from_payload(vrt::ValueType::array, arena_heap_array_payload));
  if (
    !arena_region->is_arena() ||
    (arena_region->type != vrt::RegionType::arena) ||
    (dynamic_cast<vrt::RegionArena*>(arena_region) == nullptr) ||
    (arena_heap_object->region() != arena_region) ||
    (arena_heap_array->region() != arena_region) ||
    (arena_region->stack_reference_count != 3) ||
    (arena_region->header_count() != 3))
    return 7;

  vrt_array_release(arena_heap_array_payload);
  if (
    (arena_region->stack_reference_count != 2) ||
    (arena_region->header_count() != 3) ||
    !arena_region->contains(arena_heap_array))
    return 23;

  vrt_object_release(arena_heap);
  if (
    (arena_region->stack_reference_count != 1) ||
    (arena_region->header_count() != 3) ||
    !arena_region->contains(arena_heap_object) ||
    !arena_region->contains(arena_heap_array))
    return 8;

  vrt_object_release(arena_root);

  // Moving a heap-region entry point into a frame-local object leaves that
  // region externally rooted by the field until the frame-local holder dies.
  ValuePayload frame_child_args{5};
  auto* frame_child =
    vrt_object_region(vrt::RegionType::rc, &value_class, 1, &frame_child_args);
  auto* frame_child_object = object_from_payload(frame_child);
  auto* frame_child_region = frame_child_object->region();
  HolderPayload frame_holder_args{frame_child, 8};
  auto* frame_holder = vrt_object_new(&holder_class, 2, &frame_holder_args);
  auto* frame_holder_object = object_from_payload(frame_holder);
  if (
    (frame_holder_object->region() != frame_region) ||
    (static_cast<HolderPayload*>(frame_holder)->value != frame_child) ||
    (frame_child_region->stack_reference_count != 1) ||
    frame_child_region->has_parent())
    return 9;

  vrt_object_release(frame_holder);
  if (frame_region->header_count() != 0)
    return 10;

  // A moved child region is parented at its entry object. With no remaining
  // external child reference, parent release owns both regions' teardown.
  ValuePayload child_args{6};
  auto* child =
    vrt_object_region(vrt::RegionType::rc, &value_class, 1, &child_args);
  auto* child_object = object_from_payload(child);
  auto* child_region = child_object->region();
  HolderPayload parent_args{child, 9};
  auto* parent =
    vrt_object_region(vrt::RegionType::rc, &holder_class, 2, &parent_args);
  auto* parent_object = object_from_payload(parent);
  auto* parent_region = parent_object->region();
  if (
    (child_region->parent != parent_region) ||
    (child_region->entry_point != child_object) ||
    (child_region->stack_reference_count != 0) ||
    (parent_region->stack_reference_count != 1) ||
    (static_cast<HolderPayload*>(parent)->value != child))
    return 11;

  vrt_object_release(parent);

  // An externally-retained child propagates its non-zero stack-reference
  // state into the parent. Consuming that external reference propagates the
  // transition back down while the parent field keeps the child alive.
  ValuePayload retained_child_args{10};
  auto* retained_child = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &retained_child_args);
  auto* retained_child_object = object_from_payload(retained_child);
  auto* retained_child_region = retained_child_object->region();
  vrt_object_retain(retained_child);
  HolderPayload retained_parent_args{retained_child, 11};
  auto* retained_parent = vrt_object_region(
    vrt::RegionType::rc, &holder_class, 2, &retained_parent_args);
  auto* retained_parent_object = object_from_payload(retained_parent);
  auto* retained_parent_region = retained_parent_object->region();
  if (
    (retained_child_region->parent != retained_parent_region) ||
    (retained_child_region->entry_point != retained_child_object) ||
    (retained_child_region->stack_reference_count != 1) ||
    (retained_parent_region->stack_reference_count != 2) ||
    (retained_child_object->reference_count != 2))
    return 12;

  vrt_object_release(retained_child);
  if (
    (retained_child_region->stack_reference_count != 0) ||
    (retained_parent_region->stack_reference_count != 1) ||
    (retained_child_object->reference_count != 1) ||
    (retained_child_region->parent != retained_parent_region))
    return 13;

  vrt_object_release(retained_parent);

  // Releasing the parent first collects its Holder while a retained child
  // remains live. Clearing the child's parent can drop the parent's last
  // propagated stack reference, so collection must guard against re-entrant
  // region destruction until the Holder storage is gone.
  ValuePayload guarded_child_args{16};
  auto* guarded_child = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &guarded_child_args);
  auto* guarded_child_object = object_from_payload(guarded_child);
  auto* guarded_child_region = guarded_child_object->region();
  vrt_object_retain(guarded_child);
  HolderPayload guarded_parent_args{guarded_child, 17};
  auto* guarded_parent = vrt_object_region(
    vrt::RegionType::rc, &holder_class, 2, &guarded_parent_args);
  auto* guarded_parent_region = object_from_payload(guarded_parent)->region();
  if (
    (guarded_child_region->parent != guarded_parent_region) ||
    (guarded_child_region->stack_reference_count != 1) ||
    (guarded_parent_region->stack_reference_count != 2))
    return 14;

  vrt_object_release(guarded_parent);
  if (
    guarded_child_region->has_parent() ||
    (guarded_child_region->entry_point != nullptr) ||
    (guarded_child_region->stack_reference_count != 1) ||
    guarded_child_region->destroying || guarded_child_region->is_finalizing() ||
    (guarded_child_region->header_count() != 1) ||
    !guarded_child_region->contains(guarded_child_object) ||
    (static_cast<ValuePayload*>(guarded_child)->value != 16))
    return 15;

  vrt_object_release(guarded_child);
  if (frame_region->header_count() != 0)
    return 16;

  ValuePayload cleanup_child_args{15};
  auto* cleanup_child = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &cleanup_child_args);
  auto* cleanup_child_region = object_from_payload(cleanup_child)->region();
  vrt_object_retain(cleanup_child);
  HolderPayload frame_cleanup_args{cleanup_child, 18};
  auto* frame_cleanup =
    vrt_object_new(&holder_class, 2, &frame_cleanup_args);
  if (
    (object_from_payload(frame_cleanup)->region() != frame_region) ||
    (frame_region->header_count() != 1) ||
    (cleanup_child_region->stack_reference_count != 2))
    return 24;

  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (root_frame->region != frame_region) ||
    (frame_region->header_count() != 1) ||
    (cleanup_child_region->stack_reference_count != 2))
    return 25;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != nullptr) ||
    (cleanup_child_region->stack_reference_count != 1) ||
    cleanup_child_region->has_parent() ||
    (static_cast<ValuePayload*>(cleanup_child)->value != 15))
    return 26;

  vrt_object_release(cleanup_child);
  vrt_thread_deinit();
  if (vrt_thread_current() != nullptr)
    return 17;

  return 0;
}
