// Coverage:
// - Constructs register, field, and array references through the public ABI.
// - Checks owner retain/release, return and raise escape, frame epochs, and
//   tailcall validation.
//
// Non-goals:
// - Loading and exchanging referenced values are covered by the follow-up
//   reference operation fixture.

#include "array.h"
#include "frame.h"
#include "object.h"
#include "region.h"
#include "value.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/program.h>
#include <vrt/reference.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t scalar_array_type_id = 0x201;
  constexpr uintptr_t scalar_reference_type_id = 0x301;
  constexpr uintptr_t value_field_id = 0x401;

  struct ValueFields
  {
    uint64_t value;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     scalar_type_id,
     vrt::ValueType::scalar,
     value_field_id}};

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

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint64_t), 0},
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {scalar_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_type_id},
    {scalar_reference_type_id,
     vrt::ValueType::reference,
     sizeof(vrt::Reference),
     0}};
  const vrt::Program program{4, types, 0, nullptr};

  const vrt::Function root_function{1, "root", nullptr};
  const vrt::Function child_function{2, "child", nullptr};
  const vrt::Function tail_function{3, "tail", nullptr};

  void reject_current_frame_escape(void*)
  {
    auto* frame = vrt_frame_enter(&root_function);
    uint64_t slot = 1;
    vrt::Reference reference{};
    vrt_reference_from_register(
      &reference, frame, &slot, scalar_type_id);
    vrt_reference_escape(&reference);
  }

  void reject_current_frame_tailcall(void*)
  {
    auto* frame = vrt_frame_enter(&root_function);
    uint64_t slot = 1;
    vrt::Reference reference{};
    vrt_reference_from_register(
      &reference, frame, &slot, scalar_type_id);
    vrt_reference_validate_tailcall(&reference);
  }

  void reject_stale_register_reference(void*)
  {
    auto* frame = vrt_frame_enter(&root_function);
    uint64_t slot = 1;
    vrt::Reference reference{};
    vrt_reference_from_register(
      &reference, frame, &slot, scalar_type_id);
    vrt_frame_reuse(&tail_function);
    vrt_reference_retain(&reference);
  }

  void reject_raised_register_reference(void*)
  {
    auto* root = vrt_frame_enter(&root_function);
    auto* continuation =
      static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
    if (setjmp(*continuation) != 0)
      vrt_error_raise(VRT_ERROR_BAD_REFERENCE_TARGET);

    auto* owner = vrt_frame_enter(&child_function);
    uint64_t slot = 1;
    vrt::Reference reference{};
    vrt_reference_from_register(
      &reference, owner, &slot, scalar_type_id);
    auto* raiser = vrt_frame_enter(&child_function);
    (void)raiser;
    vrt_frame_set_raise_target(root->frame_id.raw());
    vrt_frame_raise(scalar_reference_type_id, &reference);
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt_thread_init();

  auto* root = vrt_frame_enter(&root_function);
  auto* root_region = root->region;

  ValueFields object_args{10};
  auto* object_data = vrt_object_new(&value_class, 1, &object_args);
  auto* object = static_cast<vrt::Object*>(
    vrt::Value{vrt::ValueType::object, object_data}.header());
  vrt::Reference field_reference{};
  vrt_reference_from_field(
    &field_reference, object_data, value_field_id);
  if (
    (field_reference.kind_flags !=
     static_cast<uintptr_t>(vrt::ReferenceKind::object_field)) ||
    (field_reference.owner != object_data) ||
    (field_reference.target !=
     &static_cast<ValueFields*>(object_data)->value) ||
    (field_reference.content_type_id != scalar_type_id))
    return 1;

  uint64_t loaded = 0;
  vrt_reference_load(&field_reference, &loaded);
  if (loaded != 10)
    return 15;
  const uint64_t replacement = 20;
  uint64_t previous = 0;
  vrt_reference_exchange(
    &field_reference, &replacement, &previous);
  if (
    (previous != 10) ||
    (static_cast<ValueFields*>(object_data)->value != 20))
    return 16;

  vrt_reference_retain(&field_reference);
  if (object->get_rc() != 2)
    return 2;
  vrt_reference_release(&field_reference);
  if (object->get_rc() != 1)
    return 3;
  vrt_reference_release(&field_reference);
  if (root_region->header_count() != 0)
    return 4;

  auto* array_data = vrt_array_new(scalar_array_type_id, 2);
  auto* array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, array_data}.header());
  vrt::Reference array_reference{};
  vrt_reference_from_array(&array_reference, array_data, 1);
  if (
    (array_reference.kind_flags !=
     static_cast<uintptr_t>(vrt::ReferenceKind::array_element)) ||
    (array_reference.owner != array_data) ||
    (array_reference.target != array->load(1)) ||
    (array_reference.content_type_id != scalar_type_id))
    return 5;
  *static_cast<uint64_t*>(array_reference.target) = 30;
  vrt_reference_load(&array_reference, &loaded);
  if (loaded != 30)
    return 17;
  const uint64_t array_replacement = 40;
  vrt_reference_exchange(
    &array_reference, &array_replacement, &previous);
  if (
    (previous != 30) ||
    (*static_cast<uint64_t*>(array_reference.target) != 40))
    return 18;

  vrt_reference_retain(&array_reference);
  if (array->get_rc() != 2)
    return 6;
  vrt_reference_release(&array_reference);
  vrt_reference_release(&array_reference);
  if (root_region->header_count() != 0)
    return 7;

  uint64_t root_slot = 4;
  vrt::Reference register_reference{};
  vrt_reference_from_register(
    &register_reference, root, &root_slot, scalar_type_id);
  vrt_reference_load(&register_reference, &loaded);
  if (loaded != 4)
    return 19;
  const uint64_t register_replacement = 5;
  vrt_reference_exchange(
    &register_reference, &register_replacement, &previous);
  if ((previous != 4) || (root_slot != 5))
    return 20;

  auto* child = vrt_frame_enter(&child_function);
  vrt_reference_escape(&register_reference);
  vrt_reference_validate_tailcall(&register_reference);
  vrt_frame_leave();
  if ((vrt_thread_current_frame() != root) || (child == root))
    return 8;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 9;

  if (setjmp(*continuation) == 0)
  {
    auto* intermediate = vrt_frame_enter(&child_function);
    ValueFields raised_args{11};
    auto* raised_object_data =
      vrt_object_new(&value_class, 1, &raised_args);
    vrt::Reference raised_reference{};
    vrt_reference_from_field(
      &raised_reference, raised_object_data, value_field_id);
    auto* raise_frame = vrt_frame_enter(&child_function);
    if (
      (vrt_frame_set_raise_target(root->frame_id.raw()) !=
       raise_frame->frame_id.raw()) ||
      (intermediate->region == root_region))
      return 10;

    vrt_frame_raise(scalar_reference_type_id, &raised_reference);
  }

  vrt::Reference raised_reference{};
  vrt_frame_take_raised_value(
    scalar_reference_type_id, &raised_reference);
  auto* raised_object = static_cast<vrt::Object*>(
    vrt::Value{vrt::ValueType::object, raised_reference.owner}.header());
  if (
    (vrt_thread_current_frame() != root) ||
    (raised_object->region() != root_region) ||
    (static_cast<ValueFields*>(raised_reference.owner)->value != 11))
    return 11;
  vrt_reference_load(&raised_reference, &loaded);
  if (loaded != 11)
    return 21;
  vrt_reference_release(&raised_reference);

  vrt_frame_leave();

  vrt_error_info error{};
  if (
    vrt_try_invoke(reject_current_frame_escape, nullptr, &error) ||
    (error.code != VRT_ERROR_BAD_STACK_ESCAPE))
    return 12;
  if (
    vrt_try_invoke(reject_current_frame_tailcall, nullptr, &error) ||
    (error.code != VRT_ERROR_BAD_STACK_ESCAPE))
    return 13;
  if (
    vrt_try_invoke(reject_stale_register_reference, nullptr, &error) ||
    (error.code != VRT_ERROR_BAD_REFERENCE_TARGET))
    return 14;
  if (
    vrt_try_invoke(reject_raised_register_reference, nullptr, &error) ||
    (error.code != VRT_ERROR_BAD_STACK_ESCAPE))
    return 22;

  vrt_thread_deinit();
  return 0;
}
