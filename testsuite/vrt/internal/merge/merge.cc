// See README.md for merge coverage and runtime boundaries.

#include "frame.h"
#include "header.h"
#include "region.h"
#include "region_ext.h"
#include "value.h"
#include "vrt.h"
#include "writebarrier.h"

#include <cstddef>
#include <cstdint>
#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/region.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t holder_class_id = 0x102;
  constexpr uintptr_t scalar_array_type_id = 0x200;

  struct ValueFields
  {
    uint64_t value;
  };

  struct HolderFields
  {
    void* next;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     scalar_type_id,
     vrt::ValueType::scalar}};

  const vrt::Field holder_fields[] = {
    {offsetof(HolderFields, next),
     sizeof(HolderFields::next),
     value_class_id,
     vrt::ValueType::object}};

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
    1,
    holder_fields,
    0,
    nullptr,
    nullptr};

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint64_t), 0},
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {holder_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {scalar_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_type_id}};
  const vrt::Program program{4, types, 0, nullptr};
  const vrt::Function root_function{1, "root", nullptr};

  vrt::Header* header(void* data)
  {
    return vrt::Value{vrt::ValueType::object, data}.header();
  }

  vrt::Header* array_header(void* elements)
  {
    return vrt::Value{vrt::ValueType::array, elements}.header();
  }

  void* new_region_object(vrt::RegionType type = vrt::RegionType::rc)
  {
    ValueFields fields{42};
    return vrt_object_region(type, &value_class, 1, &fields);
  }

  void* new_region_holder()
  {
    auto* region = vrt::Region::create(vrt::RegionType::rc);
    return region->object(&holder_class)->data();
  }

  void set_child(void* parent_data, void* child_data)
  {
    auto* parent = header(parent_data);
    auto* fields = static_cast<HolderFields*>(parent_data);
    vrt::writebarrier::init(
      parent->location(), &fields->next, holder_fields[0], &child_data);
  }

  struct MergeCall
  {
    void* left;
    void* right;
  };

  void invoke_merge(void* raw_context)
  {
    auto* context = static_cast<MergeCall*>(raw_context);
    vrt_region_merge(
      vrt::ValueType::object,
      context->left,
      vrt::ValueType::object,
      context->right);
  }

  struct StackMergeCall
  {
    void* heap;
  };

  void invoke_stack_merge(void* raw_context)
  {
    auto* context = static_cast<StackMergeCall*>(raw_context);
    vrt_frame_enter(&root_function);
    ValueFields fields{42};
    auto* stack = vrt_object_stack(&value_class, 1, &fields);
    vrt_region_merge(
      vrt::ValueType::object,
      stack,
      vrt::ValueType::object,
      context->heap);
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt_thread_init();

  auto* same_left = new_region_object();
  auto* same_region = header(same_left)->region();
  ValueFields fields{7};
  auto* same_right =
    vrt_object_heap(same_left, &value_class, 1, static_cast<void*>(&fields));
  vrt_region_merge(
    vrt::ValueType::object,
    same_left,
    vrt::ValueType::object,
    same_right);
  if (
    (header(same_left)->region() != same_region) ||
    (header(same_right)->region() != same_region) ||
    (same_region->stack_reference_count != 2) ||
    (same_region->header_count() != 2))
    return 1;
  vrt_object_release(same_right);
  vrt_object_release(same_left);

  auto* left = new_region_object();
  auto* right = vrt_array_region(
    vrt::RegionType::rc, scalar_array_type_id, uintptr_t{2});
  auto* destination = header(left)->region();
  vrt_region_merge(
    vrt::ValueType::object,
    left,
    vrt::ValueType::array,
    right);
  if (
    (header(left)->region() != destination) ||
    (array_header(right)->region() != destination) ||
    (destination->stack_reference_count != 2) ||
    (destination->header_count() != 2))
    return 2;
  vrt_array_release(right);
  vrt_object_release(left);

  auto* outer_frame = vrt_frame_enter(&root_function);
  ValueFields outer_fields{1};
  auto* outer = vrt_object_new(&value_class, 1, &outer_fields);
  auto* outer_region = outer_frame->region;
  auto* inner_frame = vrt_frame_enter(&root_function);
  ValueFields inner_fields{2};
  auto* inner = vrt_object_new(&value_class, 1, &inner_fields);
  auto* inner_region = inner_frame->region;
  vrt_region_merge(
    vrt::ValueType::object,
    outer,
    vrt::ValueType::object,
    inner);
  if (
    (outer_region == inner_region) ||
    (header(outer)->region() != outer_region) ||
    (header(inner)->region() != inner_region))
    return 16;
  vrt_object_release(inner);
  vrt_frame_leave();
  vrt_object_release(outer);
  vrt_frame_leave();

  auto* frame = vrt_frame_enter(&root_function);
  ValueFields frame_fields{3};
  auto* frame_local = vrt_object_new(&value_class, 1, &frame_fields);
  auto* heap = new_region_object();
  auto* heap_region = header(heap)->region();
  vrt_region_merge(
    vrt::ValueType::object,
    frame_local,
    vrt::ValueType::object,
    heap);
  if (
    (header(frame_local)->region() != heap_region) ||
    frame->region->contains(header(frame_local)) ||
    !heap_region->contains(header(frame_local)) ||
    (heap_region->stack_reference_count != 2) ||
    (heap_region->header_count() != 2))
    return 3;
  vrt_object_release(frame_local);
  vrt_object_release(heap);
  vrt_frame_leave();

  auto* destination_data = new_region_object();
  destination = header(destination_data)->region();
  auto* source_parent_data = new_region_holder();
  auto* source_region = header(source_parent_data)->region();
  auto* child_data = new_region_object();
  auto* child_region = header(child_data)->region();
  set_child(source_parent_data, child_data);
  if (
    (child_region->parent != source_region) ||
    (child_region->stack_reference_count != 0))
    return 4;
  vrt_region_merge(
    vrt::ValueType::object,
    destination_data,
    vrt::ValueType::object,
    source_parent_data);
  if (
    (header(source_parent_data)->region() != destination) ||
    (child_region->parent != destination) ||
    (child_region->entry_point != header(child_data)) ||
    (child_region->stack_reference_count != 0) ||
    (destination->stack_reference_count != 2) ||
    (destination->header_count() != 2))
    return 5;
  vrt_object_release(source_parent_data);
  vrt_object_release(destination_data);

  auto* owner_data = new_region_holder();
  auto* owner_region = header(owner_data)->region();
  auto* owned_data = new_region_object();
  auto* owned_region = header(owned_data)->region();
  set_child(owner_data, owned_data);
  auto* unowned_data = new_region_object();
  vrt_region_merge(
    vrt::ValueType::object,
    unowned_data,
    vrt::ValueType::object,
    owned_data);
  if (
    (header(unowned_data)->region() != owned_region) ||
    (owned_region->parent != owner_region) ||
    (owned_region->entry_point != header(owned_data)) ||
    (owned_region->stack_reference_count != 1) ||
    (owned_region->header_count() != 2))
    return 6;
  vrt_object_release(unowned_data);
  vrt_object_release(owner_data);

  auto* first_owner_data = new_region_holder();
  auto* first_owned_data = new_region_object();
  auto* first_owner_region = header(first_owner_data)->region();
  auto* first_owned_region = header(first_owned_data)->region();
  set_child(first_owner_data, first_owned_data);
  auto* second_owner_data = new_region_holder();
  auto* second_owned_data = new_region_object();
  auto* second_owner_region = header(second_owner_data)->region();
  auto* second_owned_region = header(second_owned_data)->region();
  set_child(second_owner_data, second_owned_data);

  MergeCall both_owned{first_owned_data, second_owned_data};
  vrt::ErrorInfo error{};
  if (vrt_try_invoke(invoke_merge, &both_owned, &error))
    return 7;
  if (
    (error.code != vrt::Error::bad_merge) ||
    (header(first_owned_data)->region() != first_owned_region) ||
    (header(second_owned_data)->region() != second_owned_region) ||
    (first_owned_region->parent != first_owner_region) ||
    (second_owned_region->parent != second_owner_region) ||
    (first_owned_region->stack_reference_count != 0) ||
    (second_owned_region->stack_reference_count != 0) ||
    (first_owner_region->stack_reference_count != 1) ||
    (second_owner_region->stack_reference_count != 1))
    return 8;
  vrt_object_release(first_owner_data);
  vrt_object_release(second_owner_data);

  auto* ancestor_data = new_region_holder();
  auto* descendant_data = new_region_object();
  auto* ancestor_region = header(ancestor_data)->region();
  auto* descendant_region = header(descendant_data)->region();
  set_child(ancestor_data, descendant_data);
  MergeCall ancestry{ancestor_data, descendant_data};
  if (vrt_try_invoke(invoke_merge, &ancestry, &error))
    return 9;
  if (
    (error.code != vrt::Error::bad_merge) ||
    (header(ancestor_data)->region() != ancestor_region) ||
    (header(descendant_data)->region() != descendant_region) ||
    (descendant_region->parent != ancestor_region) ||
    (descendant_region->stack_reference_count != 0) ||
    (ancestor_region->stack_reference_count != 1))
    return 10;
  vrt_object_release(ancestor_data);

  auto* stack_target = new_region_object();
  StackMergeCall stack_merge{stack_target};
  if (vrt_try_invoke(invoke_stack_merge, &stack_merge, &error))
    return 11;
  if (
    (error.code != vrt::Error::bad_stack_escape) ||
    (vrt_thread_current_frame() != nullptr) ||
    (header(stack_target)->region() == nullptr) ||
    (header(stack_target)->region()->stack_reference_count != 1))
    return 12;
  vrt_object_release(stack_target);

  auto* first_arena = new_region_object(vrt::RegionType::arena);
  auto* second_arena = new_region_object(vrt::RegionType::arena);
  auto* first_arena_region = header(first_arena)->region();
  auto* second_arena_region = header(second_arena)->region();
  MergeCall arenas{first_arena, second_arena};
  if (vrt_try_invoke(invoke_merge, &arenas, &error))
    return 13;
  if (
    (error.code != vrt::Error::bad_merge) ||
    (header(first_arena)->region() != first_arena_region) ||
    (header(second_arena)->region() != second_arena_region) ||
    (first_arena_region->stack_reference_count != 1) ||
    (second_arena_region->stack_reference_count != 1) ||
    (first_arena_region->header_count() != 1) ||
    (second_arena_region->header_count() != 1))
    return 14;
  vrt_object_release(first_arena);
  vrt_object_release(second_arena);

  vrt_thread_deinit();
  return vrt_thread_current() == nullptr ? 0 : 15;
}
