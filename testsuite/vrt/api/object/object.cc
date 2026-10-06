// See README.md for the object-runtime coverage and test boundaries.

#include "object.h"

#include "frame.h"
#include "region.h"
#include "region_rc.h"
#include "value.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vrt/program.h>
#include <vrt/thread.h>

static_assert(std::is_base_of_v<vrt::Header, vrt::Object>);

namespace
{
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t empty_class_id = 0x102;
  constexpr uintptr_t singleton_class_id = 0x103;

  struct alignas(16) ValueFields
  {
    uint64_t value;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     0,
     vrt::ValueType::scalar}};

  void singleton_first_method() {}
  void singleton_finalizer() {}
  void singleton_method() {}
  void singleton_last_method() {}
  const vrt::Function singleton_first_function{
    0x300, "Singleton.first", &singleton_first_method};
  const vrt::Function singleton_finalizer_function{
    0x303, "Singleton.final", &singleton_finalizer};
  const vrt::Function singleton_function{
    0x301, "Singleton.method", &singleton_method};
  const vrt::Function singleton_last_function{
    0x302, "Singleton.last", &singleton_last_method};
  const vrt::Method singleton_methods[] = {
    {vrt::Class::final_method_id, &singleton_finalizer_function},
    {0x101, &singleton_first_function},
    {0x201, &singleton_function},
    {0x301, &singleton_last_function}};

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

  const vrt::Class empty_class{
    empty_class_id, "Empty", 0, 1, 0, nullptr, 0, nullptr, nullptr};

  alignas(vrt::Object) std::byte
    singleton_storage[vrt::Object::singleton_storage_size()]{};

  const vrt::Class singleton_class{
    singleton_class_id,
    "Singleton",
    0,
    1,
    0,
    nullptr,
    4,
    singleton_methods,
    singleton_storage + vrt::Object::singleton_data_offset()};

  const vrt::TypeInfo types[] = {
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {empty_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {singleton_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Singleton singletons[] = {{singleton_storage, &singleton_class}};
  const vrt::Program program{3, types, 1, singletons};

  vrt::Object* object_from_data(void* data_address)
  {
    return static_cast<vrt::Object*>(
      vrt::Value{vrt::ValueType::object, data_address}.header());
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);

  if (
    (value_class.id != value_class_id) ||
    (value_class.data_size != sizeof(ValueFields)) ||
    (value_class.data_alignment != alignof(ValueFields)) ||
    (value_class.field_count != 1) || (value_class.fields != value_fields) ||
    (singleton_class.method_count != 4) ||
    (singleton_class.methods != singleton_methods) ||
    (singleton_methods[0].id != vrt::Class::final_method_id) ||
    (singleton_methods[1].id != 0x101) || (singleton_methods[2].id != 0x201) ||
    (singleton_methods[3].id != 0x301) ||
    (singleton_methods[2].func != &singleton_function) ||
    (singleton_class.method(0x101) != &singleton_first_function) ||
    (singleton_class.method(0x202) != nullptr) ||
    (singleton_class.finalizer() != &singleton_finalizer_function))
    return 1;

  const vrt::Function root_function{1, "root", nullptr};
  const vrt::Function child_function{2, "child", nullptr};
  const vrt::Function intermediate_function{3, "intermediate", nullptr};
  vrt_thread_init();
  auto* root_frame = vrt_frame_enter(&root_function);
  if ((root_frame == nullptr) || (root_frame->region == nullptr))
    return 2;

  auto* frame_region = root_frame->region;

  // New allocates in the current frame's region and copies the
  // field-layout argument packet through the initialization barrier.
  ValueFields frame_object_args{42};
  auto* frame_object_data = vrt_object_new(&value_class, 1, &frame_object_args);
  auto* frame_object = object_from_data(frame_object_data);
  if (
    (frame_object_data == nullptr) ||
    ((reinterpret_cast<uintptr_t>(frame_object_data) % alignof(ValueFields)) !=
     0) ||
    (static_cast<ValueFields*>(frame_object_data)->value != 42) ||
    (frame_object->value_type() != vrt::ValueType::object) ||
    (reinterpret_cast<std::byte*>(frame_object_data) - sizeof(vrt::Object) !=
     reinterpret_cast<std::byte*>(frame_object)) ||
    (frame_object->cls != &value_class) ||
    (frame_object->fields() != frame_object_data) ||
    (vrt::Value{vrt::ValueType::object, frame_object_data}.header() !=
     frame_object) ||
    (vrt::Value{vrt::ValueType::object, frame_object_data}.type() !=
     vrt::ValueType::object) ||
    (vrt::Value{vrt::ValueType::object, frame_object_data}.location() !=
     vrt::Location(frame_region)) ||
    (vrt::Value{vrt::ValueType::object, frame_object_data}.region() !=
     frame_region) ||
    (vrt::Value{vrt::ValueType::scalar, nullptr}.location() !=
     vrt::Location::immortal()) ||
    (frame_object->data() != frame_object_data) ||
    (frame_object->allocation == nullptr) ||
    (frame_object->region() != frame_region) ||
    (frame_object->location() != vrt::Location(frame_region)) ||
    (vrt_object_class_id(frame_object_data) != value_class_id) ||
    frame_object->finalizing || (frame_object->reference_count != 1) ||
    (frame_region->header_count() != 1) ||
    !frame_region->contains(frame_object))
    return 3;

  vrt_object_retain(frame_object_data);
  if (frame_object->reference_count != 2)
    return 4;

  vrt_object_release(frame_object_data);
  if (
    (frame_object->reference_count != 1) ||
    !frame_region->contains(frame_object))
    return 5;

  vrt_object_release(frame_object_data);
  if (frame_region->header_count() != 0)
    return 6;

  ValueFields region_object_args{17};
  auto* region_object_data = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &region_object_args);
  auto* region_object = object_from_data(region_object_data);
  if (
    (static_cast<ValueFields*>(region_object_data)->value != 17) ||
    (region_object->cls != &value_class) ||
    (region_object->region() == nullptr) ||
    region_object->region()->is_frame_local() ||
    (dynamic_cast<vrt::RegionRC*>(region_object->region()) == nullptr) ||
    (region_object->reference_count != 1) ||
    (region_object->region()->header_count() != 1))
    return 17;

  ValueFields heap_object_args{18};
  auto* heap_object_data =
    vrt_object_heap(region_object_data, &value_class, 1, &heap_object_args);
  auto* heap_object = object_from_data(heap_object_data);
  if (
    (static_cast<ValueFields*>(heap_object_data)->value != 18) ||
    (heap_object->cls != &value_class) ||
    (heap_object->region() != region_object->region()) ||
    (heap_object->reference_count != 1) ||
    (region_object->region()->header_count() != 2))
    return 21;

  vrt_object_release(heap_object_data);
  if (region_object->region()->header_count() != 1)
    return 22;

  // Empty classes still allocate real objects. Singleton metadata provides a
  // separate immortal value only for explicit singleton operations.
  auto* empty_new_first_data =
    vrt_object_new(&singleton_class, 0, nullptr);
  auto* empty_new_second_data =
    vrt_object_new(&singleton_class, 0, nullptr);
  auto* empty_new_first = object_from_data(empty_new_first_data);
  auto* empty_new_second = object_from_data(empty_new_second_data);
  if (
    (empty_new_first_data == empty_new_second_data) ||
    (empty_new_first_data == singleton_class.singleton) ||
    (empty_new_second_data == singleton_class.singleton) ||
    (empty_new_first->region() != frame_region) ||
    (empty_new_second->region() != frame_region) ||
    (frame_region->header_count() != 2))
    return 23;

  auto* plain_empty_data = vrt_object_new(&empty_class, 0, nullptr);
  auto* plain_empty = object_from_data(plain_empty_data);
  if (
    (plain_empty->cls != &empty_class) ||
    (plain_empty->region() != frame_region) ||
    (frame_region->header_count() != 3))
    return 24;

  auto* empty_stack_data =
    vrt_object_stack(&singleton_class, 0, nullptr);
  auto* empty_stack = object_from_data(empty_stack_data);
  if (
    (empty_stack_data == singleton_class.singleton) ||
    !empty_stack->location().is_stack() ||
    (empty_stack->cls != &singleton_class) ||
    (frame_region->header_count() != 3))
    return 25;

  auto* empty_heap_data =
    vrt_object_heap(region_object_data, &singleton_class, 0, nullptr);
  auto* empty_heap = object_from_data(empty_heap_data);
  if (
    (empty_heap_data == singleton_class.singleton) ||
    (empty_heap->region() != region_object->region()) ||
    (empty_heap->cls != &singleton_class) ||
    (region_object->region()->header_count() != 2))
    return 26;

  auto* empty_region_data =
    vrt_object_region(vrt::RegionType::rc, &empty_class, 0, nullptr);
  auto* empty_region = object_from_data(empty_region_data);
  if (
    (empty_region->region() == nullptr) ||
    empty_region->region()->is_frame_local() ||
    empty_region->region()->is_arena() ||
    (empty_region->region() == region_object->region()) ||
    (empty_region->region()->header_count() != 1))
    return 27;

  auto* empty_arena_data =
    vrt_object_region(vrt::RegionType::arena, &empty_class, 0, nullptr);
  auto* empty_arena = object_from_data(empty_arena_data);
  if (
    (empty_arena->region() == nullptr) ||
    !empty_arena->region()->is_arena() ||
    (empty_arena->region() == empty_region->region()) ||
    (empty_arena->region()->header_count() != 1))
    return 28;

  vrt_object_release(empty_region_data);
  vrt_object_release(empty_arena_data);
  vrt_object_release(empty_heap_data);
  vrt_object_release(empty_new_first_data);
  vrt_object_release(empty_new_second_data);
  vrt_object_release(plain_empty_data);
  if (
    (region_object->region()->header_count() != 1) ||
    (frame_region->header_count() != 0))
    return 29;

  auto* singleton_object_data = singleton_class.singleton;
  auto* singleton_object = object_from_data(singleton_object_data);
  if (
    (singleton_object->location() != vrt::Location::immortal()) ||
    (singleton_object->region() != nullptr) ||
    (singleton_object->cls != &singleton_class) ||
    (vrt_object_class_id(singleton_object_data) != singleton_class_id) ||
    (singleton_object->reference_count != 1))
    return 7;

  auto* finalizer =
    vrt_object_lookup(singleton_object_data, vrt::Class::final_method_id);
  auto* first_callable = vrt_object_lookup(singleton_object_data, 0x101);
  auto* callable = vrt_object_lookup(singleton_object_data, 0x201);
  auto* last_callable = vrt_object_lookup(singleton_object_data, 0x301);
  if (
    (finalizer != &singleton_finalizer_function) ||
    (first_callable != &singleton_first_function) ||
    (callable != &singleton_function) ||
    (last_callable != &singleton_last_function) ||
    (vrt_object_lookup(singleton_object_data, 0x202) != nullptr))
    return 8;

  vrt_object_retain(singleton_object_data);
  vrt_object_escape(singleton_object_data);
  vrt_object_release(singleton_object_data);
  if (
    (singleton_class.singleton != singleton_object_data) ||
    (singleton_object->reference_count != 1))
    return 9;

  vrt_object_release(region_object_data);
  if (frame_region->header_count() != 0)
    return 10;

  auto* child_frame = vrt_frame_enter(&child_function);
  ValueFields escaped_object_args{12};
  auto* escaped_object_data =
    vrt_object_new(&value_class, 1, &escaped_object_args);
  auto* escaped_object = object_from_data(escaped_object_data);
  auto* callee_region = child_frame->region;
  if (
    (callee_region == nullptr) || (callee_region == frame_region) ||
    (escaped_object->region() != callee_region) ||
    !callee_region->contains(escaped_object))
    return 11;

  vrt_object_escape(escaped_object_data);
  if (
    (escaped_object->region() != frame_region) ||
    !frame_region->contains(escaped_object) ||
    callee_region->contains(escaped_object) ||
    (callee_region->header_count() != 0))
    return 12;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (static_cast<ValueFields*>(escaped_object_data)->value != 12) ||
    (escaped_object->region() != frame_region) ||
    !frame_region->contains(escaped_object))
    return 13;

  vrt_object_release(escaped_object_data);
  if (frame_region->header_count() != 0)
    return 14;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 15;

  if (setjmp(*continuation) == 0)
  {
    auto* intermediate_frame = vrt_frame_enter(&intermediate_function);
    ValueFields raised_object_args{13};
    auto* raised_object_data =
      vrt_object_new(&value_class, 1, &raised_object_args);
    auto* raised_object = object_from_data(raised_object_data);
    auto* intermediate_region = intermediate_frame->region;
    auto* raise_frame = vrt_frame_enter(&child_function);
    if (
      (vrt_frame_set_raise_target(root_frame->frame_id.raw()) !=
       raise_frame->frame_id.raw()) ||
      (raised_object->region() != intermediate_region) ||
      !intermediate_region->contains(raised_object))
      return 16;

    vrt_frame_raise(value_class_id, &raised_object_data);
  }

  void* raised_object_data = nullptr;
  vrt_frame_take_raised_value(value_class_id, &raised_object_data);
  auto* raised_object = object_from_data(raised_object_data);
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (raised_object->region() != frame_region) ||
    (static_cast<ValueFields*>(raised_object_data)->value != 13) ||
    !frame_region->contains(raised_object))
    return 18;

  vrt_object_release(raised_object_data);
  if (frame_region->header_count() != 0)
    return 19;

  vrt_frame_leave();
  vrt_thread_deinit();
  if (vrt_thread_current() != nullptr)
    return 20;

  return 0;
}
