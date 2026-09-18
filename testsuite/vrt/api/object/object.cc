#include "object.h"

#include "frame.h"
#include "region.h"
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
  constexpr uintptr_t singleton_class_id = 0x103;

  struct alignas(16) ValuePayload
  {
    uint64_t value;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValuePayload, value),
     sizeof(ValuePayload::value),
     0,
     vrt::ValueType::scalar}};

  void singleton_first_method() {}
  void singleton_method() {}
  void singleton_last_method() {}
  const vrt::Func singleton_first_function{
    0x300, "Singleton.first", &singleton_first_method};
  const vrt::Func singleton_function{
    0x301, "Singleton.method", &singleton_method};
  const vrt::Func singleton_last_function{
    0x302, "Singleton.last", &singleton_last_method};
  const vrt::Method singleton_methods[] = {
    {0x101, &singleton_first_function},
    {0x201, &singleton_function},
    {0x301, &singleton_last_function}};

  const vrt::Class value_class{
    value_class_id,
    "Value",
    sizeof(ValuePayload),
    alignof(ValuePayload),
    1,
    value_fields,
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
    3,
    singleton_methods,
    singleton_storage + vrt::Object::singleton_payload_offset()};

  const vrt::TypeInfo types[] = {
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {singleton_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Singleton singletons[] = {{singleton_storage, &singleton_class}};
  const vrt::Program program{2, types, 1, singletons};

  vrt::Object* object_from_payload(void* payload)
  {
    return static_cast<vrt::Object*>(
      vrt::Value{vrt::ValueType::object, payload}.header());
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);

  if (
    (value_class.id != value_class_id) ||
    (value_class.payload_size != sizeof(ValuePayload)) ||
    (value_class.payload_alignment != alignof(ValuePayload)) ||
    (value_class.field_count != 1) || (value_class.fields != value_fields) ||
    (singleton_class.method_count != 3) ||
    (singleton_class.methods != singleton_methods) ||
    (singleton_methods[0].id != 0x101) || (singleton_methods[1].id != 0x201) ||
    (singleton_methods[2].id != 0x301) ||
    (singleton_methods[1].func != &singleton_function))
    return 1;

  const vrt::Func root_function{1, "root", nullptr};
  const vrt::Func child_function{2, "child", nullptr};
  const vrt::Func intermediate_function{3, "intermediate", nullptr};
  vrt_thread_init();
  auto* root_frame = vrt_frame_enter(&root_function);
  if ((root_frame == nullptr) || (root_frame->region == nullptr))
    return 2;

  auto* frame_region = root_frame->region;

  // New allocates in the current frame's region and copies the
  // payload-shaped field packet through the initialization barrier.
  ValuePayload new_args{42};
  auto* new_value = vrt_object_new(&value_class, 1, &new_args);
  auto* new_object = object_from_payload(new_value);
  if (
    (new_value == nullptr) ||
    ((reinterpret_cast<uintptr_t>(new_value) % alignof(ValuePayload)) != 0) ||
    (static_cast<ValuePayload*>(new_value)->value != 42) ||
    (new_object->value_type() != vrt::ValueType::object) ||
    (reinterpret_cast<std::byte*>(new_value) - sizeof(vrt::Object) !=
     reinterpret_cast<std::byte*>(new_object)) ||
    (new_object->cls != &value_class) ||
    (new_object->get_payload() != new_value) ||
    (vrt::Value{vrt::ValueType::object, new_value}.header() != new_object) ||
    (vrt::Value{vrt::ValueType::object, new_value}.type() !=
     vrt::ValueType::object) ||
    (vrt::Value{vrt::ValueType::object, new_value}.location() !=
     vrt::Location(frame_region)) ||
    (vrt::Value{vrt::ValueType::object, new_value}.region() != frame_region) ||
    (vrt::Value{vrt::ValueType::scalar, nullptr}.location() !=
     vrt::Location::immortal()) ||
    (vrt::payload_from_header(new_object) != new_value) ||
    (new_object->allocation == nullptr) ||
    (new_object->region() != frame_region) ||
    (new_object->location() != vrt::Location(frame_region)) ||
    new_object->finalizing || (new_object->reference_count != 1) ||
    (frame_region->header_count() != 1) || !frame_region->contains(new_object))
    return 3;

  vrt_object_retain(new_value);
  if (new_object->reference_count != 2)
    return 4;

  vrt_object_release(new_value);
  if ((new_object->reference_count != 1) || !frame_region->contains(new_object))
    return 5;

  vrt_object_release(new_value);
  if (frame_region->header_count() != 0)
    return 6;

  // Empty descriptors name one compiler-managed immortal object before any
  // allocation operation. Heap creation returns that same object after
  // validating the borrowed region locator.
  ValuePayload singleton_locator_args{14};
  auto* singleton_locator = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &singleton_locator_args);
  auto* singleton_new = vrt_object_new(&singleton_class, 0, nullptr);
  auto* singleton_again = vrt_object_new(&singleton_class, 0, nullptr);
  auto* singleton_heap =
    vrt_object_heap(singleton_locator, &singleton_class, 0, nullptr);
  auto* singleton_object = object_from_payload(singleton_new);
  if (
    (singleton_new != singleton_again) || (singleton_new != singleton_heap) ||
    (singleton_class.singleton != singleton_new) ||
    (singleton_object->location() != vrt::Location::immortal()) ||
    (singleton_object->region() != nullptr) ||
    (singleton_object->cls != &singleton_class) ||
    (singleton_object->reference_count != 1))
    return 7;

  vrt_object_retain(singleton_new);
  vrt_object_escape(singleton_new);
  vrt_object_prepare_raise(singleton_new);
  vrt_object_release(singleton_again);
  vrt_object_release(singleton_heap);
  if (
    (singleton_class.singleton != singleton_new) ||
    (singleton_object->reference_count != 1))
    return 9;

  vrt_object_release(singleton_locator);
  if (frame_region->header_count() != 0)
    return 10;

  auto* child_frame = vrt_frame_enter(&child_function);
  ValuePayload escaped_args{12};
  auto* escaped = vrt_object_new(&value_class, 1, &escaped_args);
  auto* escaped_object = object_from_payload(escaped);
  auto* callee_region = child_frame->region;
  if (
    (callee_region == nullptr) || (callee_region == frame_region) ||
    (escaped_object->region() != callee_region) ||
    !callee_region->contains(escaped_object))
    return 11;

  vrt_object_escape(escaped);
  if (
    (escaped_object->region() != frame_region) ||
    !frame_region->contains(escaped_object) ||
    callee_region->contains(escaped_object) ||
    (callee_region->header_count() != 0))
    return 12;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (static_cast<ValuePayload*>(escaped)->value != 12) ||
    (escaped_object->region() != frame_region) ||
    !frame_region->contains(escaped_object))
    return 13;

  vrt_object_release(escaped);
  if (frame_region->header_count() != 0)
    return 14;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 15;

  if (setjmp(*continuation) == 0)
  {
    auto* intermediate_frame = vrt_frame_enter(&intermediate_function);
    ValuePayload raised_args{13};
    auto* raised = vrt_object_new(&value_class, 1, &raised_args);
    auto* raised_object = object_from_payload(raised);
    auto* intermediate_region = intermediate_frame->region;
    auto* raise_frame = vrt_frame_enter(&child_function);
    if (
      (vrt_frame_set_raise_target(root_frame->frame_id.raw()) !=
       raise_frame->frame_id.raw()) ||
      (raised_object->region() != intermediate_region) ||
      !intermediate_region->contains(raised_object))
      return 16;

    vrt_object_prepare_raise(raised);
    if (
      (raised_object->region() != frame_region) ||
      !frame_region->contains(raised_object) ||
      intermediate_region->contains(raised_object) ||
      (intermediate_region->header_count() != 0))
      return 17;

    vrt_frame_raise(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(raised)));
  }

  auto* raised = reinterpret_cast<void*>(
    static_cast<uintptr_t>(vrt_frame_take_raised_value()));
  auto* raised_object = object_from_payload(raised);
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (raised_object->region() != frame_region) ||
    (static_cast<ValuePayload*>(raised)->value != 13) ||
    !frame_region->contains(raised_object))
    return 18;

  vrt_object_release(raised);
  if (frame_region->header_count() != 0)
    return 19;

  vrt_frame_leave();
  vrt_thread_deinit();
  if (vrt_thread_current() != nullptr)
    return 20;

  return 0;
}
