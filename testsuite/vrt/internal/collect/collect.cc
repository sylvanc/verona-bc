// See README.md for collector coverage, runtime boundaries, and non-goals.

#include "collect.h"

#include "frame.h"
#include "header.h"
#include "object.h"
#include "region.h"
#include "thread.h"
#include "vrt.h"

#include <cstddef>
#include <cstdint>
#include <vrt/program.h>

namespace
{
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t holder_class_id = 0x102;

  const vrt::Function finalizer_function{2, "Holder.final", nullptr};

  struct ValueFields
  {
    uint64_t value;
  };

  struct HolderFields
  {
    void* value;
    uint64_t tag;
  };

  vrt::Region* expected_owner = nullptr;
  vrt::Header* expected_header = nullptr;
  vrt::Header* duplicate_header = nullptr;
  bool owner_guard_observed = false;

  void holder_finalizer_thunk(void* data_address)
  {
    vrt_frame_enter(&finalizer_function);

    auto* header =
      vrt::Header::from_data(vrt::ValueType::object, data_address);
    if (expected_owner != nullptr)
    {
      owner_guard_observed =
        (header == expected_header) && !expected_owner->contains(header) &&
        !expected_owner->destroying && !expected_owner->is_finalizing() &&
        (expected_owner->stack_reference_count == 3);

      duplicate_header->root_ref_dec();
      vrt::collect(duplicate_header);
    }

    vrt_frame_leave();
  }

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     0,
     vrt::ValueType::scalar}};

  const vrt::Field holder_fields[] = {
    {offsetof(HolderFields, value),
     sizeof(HolderFields::value),
     value_class_id,
     vrt::ValueType::object},
    {offsetof(HolderFields, tag),
     sizeof(HolderFields::tag),
     0,
     vrt::ValueType::scalar}};

  const vrt::Class value_class{
    value_class_id,
    "Value",
    sizeof(ValueFields),
    alignof(ValueFields),
    1,
    value_fields,
    0,
    nullptr,
    nullptr};

  const vrt::Class holder_class{
    holder_class_id,
    "Holder",
    sizeof(HolderFields),
    alignof(HolderFields),
    2,
    holder_fields,
    0,
    nullptr,
    nullptr,
    &holder_finalizer_thunk};

  const vrt::TypeInfo types[] = {
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {holder_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Program program{2, types, 0, nullptr};

  vrt::Object* object_from_data(void* data_address)
  {
    return static_cast<vrt::Object*>(
      vrt::Header::from_data(vrt::ValueType::object, data_address));
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt::collect(static_cast<vrt::Header*>(nullptr));
  vrt::collect(static_cast<vrt::Region*>(nullptr));

  const vrt::Function root_function{1, "root", nullptr};
  vrt::init_thread();
  auto* root_frame = vrt_frame_enter(&root_function);
  auto* frame_region = root_frame->region;

  // Releasing this parent first collects its Holder allocation. Dropping the
  // Holder field clears the retained child's parent, and releasing the
  // collector's temporary owner guard queues the now-empty parent region.
  ValueFields child_args{11};
  auto* child =
    vrt_object_region(vrt::RegionType::rc, &value_class, 1, &child_args);
  auto* child_object = object_from_data(child);
  auto* child_region = child_object->region();
  vrt_object_retain(child);

  HolderFields parent_args{child, 12};
  auto* parent =
    vrt_object_region(vrt::RegionType::rc, &holder_class, 2, &parent_args);
  auto* parent_object = object_from_data(parent);
  auto* parent_region = parent_object->region();
  duplicate_header = parent_region->object(&value_class);
  if (
    (child_region->parent != parent_region) ||
    (child_region->stack_reference_count != 1) ||
    (parent_region->stack_reference_count != 3))
    return 1;

  expected_owner = parent_region;
  expected_header = parent_object;
  vrt_object_release(parent);
  expected_owner = nullptr;
  expected_header = nullptr;
  duplicate_header = nullptr;
  if (
    !owner_guard_observed || child_region->has_parent() ||
    (child_region->entry_point != nullptr) ||
    (child_region->stack_reference_count != 1) || child_region->destroying ||
    child_region->is_finalizing() || (child_region->header_count() != 1) ||
    !child_region->contains(child_object) ||
    (static_cast<ValueFields*>(child)->value != 11))
    return 2;

  vrt_object_release(child);

  // Frame teardown finalizes its Holder before releasing storage and leaves
  // the separately retained child region alive.
  ValueFields frame_child_args{21};
  auto* frame_child =
    vrt_object_region(vrt::RegionType::rc, &value_class, 1, &frame_child_args);
  auto* frame_child_region = object_from_data(frame_child)->region();
  vrt_object_retain(frame_child);
  HolderFields frame_holder_args{frame_child, 22};
  vrt_object_new(&holder_class, 2, &frame_holder_args);
  if (
    (frame_region->header_count() != 1) ||
    (frame_child_region->stack_reference_count != 2))
    return 3;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != nullptr) ||
    (frame_child_region->stack_reference_count != 1) ||
    frame_child_region->has_parent() ||
    (static_cast<ValueFields*>(frame_child)->value != 21))
    return 4;

  vrt_object_release(frame_child);

  // Arena teardown follows the same queued region path while retaining its
  // bulk-reclamation semantics.
  vrt_frame_enter(&root_function);
  ValueFields arena_child_args{31};
  auto* arena_child = vrt_object_new(&value_class, 1, &arena_child_args);
  HolderFields arena_args{arena_child, 32};
  auto* arena =
    vrt_object_region(vrt::RegionType::arena, &holder_class, 2, &arena_args);
  auto* arena_region = object_from_data(arena)->region();
  if (!arena_region->is_arena() || (arena_region->header_count() != 2))
    return 5;

  vrt_object_release(arena);
  if (vrt_thread_current_frame()->region->header_count() != 0)
    return 6;

  vrt_frame_leave();
  vrt::deinit_thread();
  return vrt_thread_current() == nullptr ? 0 : 7;
}