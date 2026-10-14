// See README.md for pinning coverage and runtime boundaries.

#include "header.h"
#include "location.h"
#include "frame.h"
#include "region.h"
#include "value.h"
#include "vrt.h"

#include <cstddef>
#include <cstdint>
#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t scalar_array_type_id = 0x200;

  uintptr_t finalized = 0;

  struct ValueFields
  {
    uint64_t value;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     scalar_type_id,
     vrt::ValueType::scalar}};

  void finalize_value(void*)
  {
    finalized++;
  }

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
    finalize_value};

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint64_t), 0},
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {scalar_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_type_id}};

  const vrt::Program program{3, types, 0, nullptr};
  const vrt::Function root_function{1, "root", nullptr};

  vrt::Header* object_header(void* data)
  {
    return vrt::Value{vrt::ValueType::object, data}.header();
  }

  vrt::Header* array_header(void* elements)
  {
    return vrt::Value{vrt::ValueType::array, elements}.header();
  }

  void pin_stack_object(void*)
  {
    vrt_frame_enter(&root_function);
    ValueFields stack_args{9};
    auto* data =
      vrt_object_stack(&value_class, 1, static_cast<const void*>(&stack_args));
    vrt_object_pin(data);
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt_thread_init();
  auto* frame = vrt_frame_enter(&root_function);
  auto* frame_region = frame->region;

  ValueFields object_args{42};
  auto* object_data = vrt_object_new(&value_class, 1, &object_args);
  auto* object = object_header(object_data);
  vrt_object_pin(object_data);
  auto* object_region = object->region();
  if (
    (object_region == nullptr) || object_region->is_frame_local() ||
    (object_region == frame_region) || frame_region->contains(object) ||
    !object_region->contains(object) || (object->get_rc() != 2) ||
    (object_region->stack_reference_count != 2))
    return 1;

  vrt_object_release(object_data);
  if (
    (object->get_rc() != 1) ||
    (object_region->stack_reference_count != 1) ||
    !object_region->contains(object))
    return 2;
  vrt_object_unpin(object_data);
  if (finalized != 1)
    return 3;

  auto* array_elements = vrt_array_new(scalar_array_type_id, 2);
  auto* array = array_header(array_elements);
  vrt_array_pin(array_elements);
  auto* array_region = array->region();
  if (
    (array_region == nullptr) || array_region->is_frame_local() ||
    (array_region == frame_region) || frame_region->contains(array) ||
    !array_region->contains(array) || (array->get_rc() != 2) ||
    (array_region->stack_reference_count != 2))
    return 4;

  vrt_array_release(array_elements);
  if (
    (array->get_rc() != 1) || (array_region->stack_reference_count != 1) ||
    !array_region->contains(array))
    return 5;
  vrt_array_unpin(array_elements);

  ValueFields immutable_args{17};
  auto* immutable_data = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &immutable_args);
  auto* immutable = object_header(immutable_data);
  vrt_object_freeze(immutable_data);
  vrt_object_pin(immutable_data);
  if (
    (immutable->location() != vrt::Location::immutable()) ||
    (immutable->get_arc() != 2))
    return 6;
  vrt_object_unpin(immutable_data);
  if (immutable->get_arc() != 1)
    return 7;
  vrt_object_release(immutable_data);
  if (finalized != 2)
    return 8;

  vrt_frame_leave();
  vrt::ErrorInfo error{};
  if (vrt_try_invoke(pin_stack_object, nullptr, &error))
    return 9;
  if (
    (error.code != vrt::Error::bad_stack_escape) ||
    (vrt_thread_current_frame() != nullptr))
    return 10;

  vrt_thread_deinit();
  return vrt_thread_current() == nullptr ? 0 : 11;
}
