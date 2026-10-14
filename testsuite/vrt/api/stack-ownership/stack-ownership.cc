// See README.md for stack-ownership coverage and runtime boundaries.

#include "frame.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/reference.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t payload_type_id = 0x101;
  constexpr uintptr_t holder_type_id = 0x102;
  constexpr uintptr_t array_type_id = 0x200;

  struct PayloadFields
  {
    uint64_t value;
  };

  struct HolderFields
  {
    void* payload;
  };

  const vrt::Field payload_fields[] = {
    {offsetof(PayloadFields, value),
     sizeof(PayloadFields::value),
     scalar_type_id,
     vrt::ValueType::scalar}};
  const vrt::Field holder_fields[] = {
    {offsetof(HolderFields, payload),
     sizeof(HolderFields::payload),
     payload_type_id,
     vrt::ValueType::object}};

  const vrt::Class payload_class{
    payload_type_id,
    "Payload",
    sizeof(PayloadFields),
    alignof(PayloadFields),
    1,
    payload_fields,
    0,
    nullptr,
    nullptr};
  const vrt::Class holder_class{
    holder_type_id,
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
    {payload_type_id, vrt::ValueType::object, sizeof(void*), 0},
    {holder_type_id, vrt::ValueType::object, sizeof(void*), 0},
    {array_type_id, vrt::ValueType::array, sizeof(void*), scalar_type_id}};
  const vrt::Program program{4, types, 0, nullptr};
  const vrt::Function root_function{1, "root", nullptr};
  const vrt::Function child_function{2, "child", nullptr};

  void* stack_payload(uint64_t value)
  {
    PayloadFields args{value};
    return vrt_object_stack(&payload_class, 1, &args);
  }

  void reject_object_escape(void*)
  {
    (void)vrt_frame_enter(&root_function);
    vrt_object_escape(stack_payload(1));
  }

  void reject_array_escape(void*)
  {
    (void)vrt_frame_enter(&root_function);
    vrt_array_escape(vrt_array_stack(array_type_id, 1));
  }

  void reject_object_tailcall(void*)
  {
    (void)vrt_frame_enter(&root_function);
    vrt_object_validate_tailcall(stack_payload(2));
  }

  void reject_array_tailcall(void*)
  {
    (void)vrt_frame_enter(&root_function);
    vrt_array_validate_tailcall(vrt_array_stack(array_type_id, 1));
  }

  void reject_reference_escape(void*)
  {
    (void)vrt_frame_enter(&root_function);
    vrt::Reference reference{};
    vrt_reference_from_field(&reference, stack_payload(3), 0);
    vrt_reference_escape(&reference);
  }

  void reject_reference_tailcall(void*)
  {
    (void)vrt_frame_enter(&root_function);
    vrt::Reference reference{};
    vrt_reference_from_field(&reference, stack_payload(4), 0);
    vrt_reference_validate_tailcall(&reference);
  }

  void reject_stack_to_region(void*)
  {
    (void)vrt_frame_enter(&root_function);
    HolderFields args{stack_payload(5)};
    (void)vrt_object_new(&holder_class, 1, &args);
  }

  void reject_younger_stack_store(void*)
  {
    (void)vrt_frame_enter(&root_function);
    HolderFields args{stack_payload(6)};
    auto* holder = vrt_object_stack(&holder_class, 1, &args);
    vrt::Reference reference{};
    vrt_reference_from_field(&reference, holder, 0);

    (void)vrt_frame_enter(&child_function);
    auto* incoming = stack_payload(7);
    void* outgoing = nullptr;
    vrt_reference_exchange(&reference, &incoming, &outgoing);
  }

  void reject_raised_current_stack(void*)
  {
    auto* root = vrt_frame_enter(&root_function);
    auto* continuation =
      static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
    if (setjmp(*continuation) == 0)
    {
      (void)vrt_frame_enter(&child_function);
      (void)vrt_frame_set_raise_target(root->frame_id.raw());
      auto* value = stack_payload(8);
      vrt_frame_raise(payload_type_id, &value);
    }
  }

  bool rejects(vrt::InvocationFunction function)
  {
    vrt::ErrorInfo error{};
    return !vrt_try_invoke(function, nullptr, &error) &&
      (error.code == vrt::Error::bad_stack_escape) &&
      (vrt_thread_current_frame() == nullptr);
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt_thread_init();

  if (
    !rejects(reject_object_escape) || !rejects(reject_array_escape) ||
    !rejects(reject_object_tailcall) || !rejects(reject_array_tailcall) ||
    !rejects(reject_reference_escape) || !rejects(reject_reference_tailcall) ||
    !rejects(reject_raised_current_stack))
    return 1;

  vrt::ErrorInfo error{};
  if (
    vrt_try_invoke(reject_stack_to_region, nullptr, &error) ||
    (error.code != vrt::Error::bad_store) ||
    (vrt_thread_current_frame() != nullptr))
    return 2;
  if (
    vrt_try_invoke(reject_younger_stack_store, nullptr, &error) ||
    (error.code != vrt::Error::bad_store) ||
    (vrt_thread_current_frame() != nullptr))
    return 3;

  auto* root = vrt_frame_enter(&root_function);
  auto* payload = stack_payload(42);
  auto* child = vrt_frame_enter(&child_function);
  HolderFields holder_args{payload};
  auto* holder = static_cast<HolderFields*>(
    vrt_object_stack(&holder_class, 1, &holder_args));
  if ((holder->payload != payload) || (child->parent != root))
    return 4;

  vrt_object_escape(payload);
  vrt::Reference reference{};
  vrt_reference_from_field(&reference, payload, 0);
  vrt_reference_escape(&reference);
  vrt_reference_validate_tailcall(&reference);
  vrt_object_validate_tailcall(payload);
  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != root) ||
    (static_cast<PayloadFields*>(payload)->value != 42))
    return 5;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (setjmp(*continuation) == 0)
  {
    (void)vrt_frame_enter(&child_function);
    (void)vrt_frame_set_raise_target(root->frame_id.raw());
    vrt_frame_raise(payload_type_id, &payload);
  }

  void* raised = nullptr;
  vrt_frame_take_raised_value(payload_type_id, &raised);
  if ((raised != payload) || (vrt_thread_current_frame() != root))
    return 6;

  vrt_frame_leave();
  vrt_thread_deinit();
  return 0;
}