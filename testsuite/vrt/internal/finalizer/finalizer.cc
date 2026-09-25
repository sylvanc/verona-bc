// See README.md for finalizer coverage, runtime boundaries, and non-goals.

#include "frame.h"
#include "header.h"
#include "object.h"
#include "region.h"
#include "thread.h"
#include "vrt.h"
#include "writebarrier.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vrt/error.h>
#include <vrt/program.h>

namespace
{
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t finalizable_class_id = 0x102;
  constexpr uintptr_t cycle_class_id = 0x103;

  constexpr uint64_t nested_mode = 1;
  constexpr uint64_t error_mode = 2;

  struct ValueFields
  {
    uint64_t value;
  };

  struct FinalizableFields
  {
    void* child;
    uint64_t mode;
  };

  struct CycleFields
  {
    void* next;
  };

  uintptr_t finalizer_calls = 0;
  uintptr_t readable_children = 0;
  uintptr_t nested_collections = 0;
  uintptr_t cycle_calls = 0;

  const vrt::Function finalizer_function{1, "Finalizer", nullptr};
  const vrt::Function cycle_finalizer_function{2, "Cycle.final", nullptr};

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     0,
     vrt::ValueType::scalar}};

  const vrt::Field finalizable_fields[] = {
    {offsetof(FinalizableFields, child),
     sizeof(FinalizableFields::child),
     value_class_id,
     vrt::ValueType::object},
    {offsetof(FinalizableFields, mode),
     sizeof(FinalizableFields::mode),
     0,
     vrt::ValueType::scalar}};

  const vrt::Field cycle_fields[] = {
    {offsetof(CycleFields, next),
     sizeof(CycleFields::next),
     cycle_class_id,
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
    nullptr,
    nullptr};

  void finalizer_thunk(void* data_address)
  {
    vrt_frame_enter(&finalizer_function);
    auto* fields = static_cast<FinalizableFields*>(data_address);
    finalizer_calls++;

    if (fields->child != nullptr)
    {
      auto* child =
        vrt::Header::from_data(vrt::ValueType::object, fields->child);
      if (
        (child->magic == vrt::Header::magic_value) &&
        (static_cast<ValueFields*>(fields->child)->value != 0))
        readable_children++;
    }

    if (fields->mode == nested_mode)
    {
      ValueFields nested_args{99};
      auto* nested = vrt_object_new(&value_class, 1, &nested_args);
      vrt_object_release(nested);
      nested_collections++;
    }

    if (fields->mode == error_mode)
      vrt_error_raise(VRT_ERROR_BAD_STORE);

    vrt_frame_leave();
  }

  void cycle_finalizer_thunk(void* data_address)
  {
    vrt_frame_enter(&cycle_finalizer_function);
    auto* fields = static_cast<CycleFields*>(data_address);
    if (fields->next == nullptr)
      std::abort();

    auto* sibling =
      vrt::Header::from_data(vrt::ValueType::object, fields->next);
    if (sibling->magic != vrt::Header::magic_value)
      std::abort();

    cycle_calls++;
    vrt_frame_leave();
  }

  const vrt::Class finalizable_class{
    finalizable_class_id,
    "Finalizable",
    sizeof(FinalizableFields),
    alignof(FinalizableFields),
    2,
    finalizable_fields,
    0,
    nullptr,
    nullptr,
    &finalizer_thunk};

  const vrt::Class cycle_class{
    cycle_class_id,
    "Cycle",
    sizeof(CycleFields),
    alignof(CycleFields),
    1,
    cycle_fields,
    0,
    nullptr,
    nullptr,
    &cycle_finalizer_thunk};

  const vrt::TypeInfo types[] = {
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {finalizable_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {cycle_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Program program{3, types, 0, nullptr};

  void* new_value(uint64_t value)
  {
    ValueFields args{value};
    return vrt_object_new(&value_class, 1, &args);
  }

  void* new_finalizable(uint64_t value, uint64_t mode)
  {
    auto* child = new_value(value);
    FinalizableFields args{child, mode};
    return vrt_object_new(&finalizable_class, 2, &args);
  }

  void set_next(vrt::Object* source, vrt::Object* target, bool move)
  {
    auto* target_data = target->data();
    auto* fields = static_cast<CycleFields*>(source->fields());
    if (move)
      vrt::writebarrier::init(
        source->location(), &fields->next, cycle_fields[0], &target_data);
    else
      vrt::writebarrier::copy(
        source->location(), &fields->next, cycle_fields[0], &target_data);
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt::init_thread();

  const vrt::Function root_function{3, "root", nullptr};
  auto* frame = vrt_frame_enter(&root_function);

  // Individual frame-local collection runs the thunk while the child field is
  // still live. The thunk may allocate and collect in its own logical frame.
  auto* individual = new_finalizable(11, nested_mode);
  vrt_object_release(individual);
  if (
    (finalizer_calls != 1) || (readable_children != 1) ||
    (nested_collections != 1) || (frame->region->header_count() != 0) ||
    (vrt_thread_current_frame() != frame))
    return 1;

  // Frame teardown uses the same finalizer path.
  (void)new_finalizable(12, 0);
  vrt_frame_leave();
  if (
    (finalizer_calls != 2) || (readable_children != 2) ||
    (vrt_thread_current_frame() != nullptr))
    return 2;

  frame = vrt_frame_enter(&root_function);

  // Releasing an RC-region entry tears down the whole region and finalizes its
  // dragged child before releasing any storage.
  auto* rc_child = new_value(13);
  FinalizableFields rc_args{rc_child, 0};
  auto* rc_object =
    vrt_object_region(vrt::RegionType::rc, &finalizable_class, 2, &rc_args);
  vrt_object_release(rc_object);
  if ((finalizer_calls != 3) || (readable_children != 3))
    return 3;

  // Arena bulk teardown also invokes user finalizers.
  auto* arena_child = new_value(14);
  FinalizableFields arena_args{arena_child, 0};
  auto* arena_object = vrt_object_region(
    vrt::RegionType::arena, &finalizable_class, 2, &arena_args);
  vrt_object_release(arena_object);
  if ((finalizer_calls != 4) || (readable_children != 4))
    return 4;

  // Runtime errors raised by a finalizer unwind only its generated frame. The
  // object still drops its child, and the caller frame remains usable.
  auto* failing = new_finalizable(15, error_mode);
  vrt_object_release(failing);
  if (
    (finalizer_calls != 5) || (readable_children != 5) ||
    (vrt_thread_current_frame() != frame) ||
    (frame->region->header_count() != 0))
    return 5;

  auto* after_error = new_value(16);
  vrt_object_release(after_error);
  if (frame->region->header_count() != 0)
    return 6;

  // Every member of a frozen SCC is finalized before either member's storage
  // is released, so each thunk can still inspect its sibling header.
  auto* cycle_region = vrt::Region::create(vrt::RegionType::rc);
  auto* cycle_a = cycle_region->object(&cycle_class);
  auto* cycle_b = cycle_region->object(&cycle_class);
  set_next(cycle_a, cycle_b, true);
  set_next(cycle_b, cycle_a, false);
  vrt_object_freeze(cycle_a->data());
  if (
    !cycle_a->location().is_immutable() ||
    !cycle_b->location().is_immutable() ||
    (cycle_a->representative() != cycle_b->representative()))
    return 7;

  cycle_a->root_ref_dec();
  if (cycle_calls != 2)
    return 8;

  vrt_frame_leave();
  vrt::deinit_thread();
  return vrt_thread_current() == nullptr ? 0 : 9;
}
