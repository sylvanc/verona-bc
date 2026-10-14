// See README.md for stack-storage coverage and runtime boundaries.

#include "array.h"
#include "frame.h"
#include "value.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <vrt/array.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t object_type_id = 0x101;
  constexpr uintptr_t array_type_id = 0x200;

  struct alignas(64) Fields
  {
    uint64_t value;
  };

  const vrt::Field fields[] = {
    {offsetof(Fields, value),
     sizeof(Fields::value),
     scalar_type_id,
     vrt::ValueType::scalar}};

  uint64_t finalized_sum = 0;

  void finalize(void* data)
  {
    finalized_sum += static_cast<Fields*>(data)->value;
  }

  const vrt::Class cls{
    object_type_id,
    "StackObject",
    sizeof(Fields),
    alignof(Fields),
    1,
    fields,
    0,
    nullptr,
    nullptr,
    &finalize};

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint64_t), 0},
    {object_type_id, vrt::ValueType::object, sizeof(void*), 0},
    {array_type_id, vrt::ValueType::array, sizeof(void*), scalar_type_id}};
  const vrt::Program program{3, types, 0, nullptr};

  vrt::Header* header(vrt::ValueType type, void* data)
  {
    return vrt::Value{type, data}.header();
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt_thread_init();

  const vrt::Function root_function{1, "root", nullptr};
  const vrt::Function child_function{2, "child", nullptr};
  const vrt::Function tail_function{3, "tail", nullptr};
  auto* root = vrt_frame_enter(&root_function);

  Fields root_args{11};
  auto* root_data = vrt_object_stack(&cls, 1, &root_args);
  if (
    (root_data == nullptr) ||
    ((reinterpret_cast<uintptr_t>(root_data) % alignof(Fields)) != 0) ||
    (static_cast<Fields*>(root_data)->value != 11) ||
    (header(vrt::ValueType::object, root_data)->location() != root->frame_id) ||
    (header(vrt::ValueType::object, root_data)->region() != nullptr))
    return 1;

  constexpr uintptr_t array_size = 8192;
  auto* elements =
    static_cast<uint64_t*>(vrt_array_stack(array_type_id, array_size));
  auto* array =
    static_cast<vrt::Array*>(header(vrt::ValueType::array, elements));
  if (
    (array->get_size() != array_size) ||
    (array->location() != root->frame_id) || (array->region() != nullptr) ||
    (elements[0] != 0) || (elements[array_size - 1] != 0))
    return 2;

  elements[0] = 17;
  elements[array_size - 1] = 19;
  if ((elements[0] != 17) || (elements[array_size - 1] != 19))
    return 3;

  auto* child = vrt_frame_enter(&child_function);
  Fields child_args{22};
  auto* child_data = vrt_object_stack(&cls, 1, &child_args);
  if (header(vrt::ValueType::object, child_data)->location() != child->frame_id)
    return 4;

  vrt_frame_leave();
  if ((vrt_thread_current_frame() != root) || (finalized_sum != 22))
    return 5;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (setjmp(*continuation) == 0)
  {
    child = vrt_frame_enter(&child_function);
    Fields raised_args{44};
    (void)vrt_object_stack(&cls, 1, &raised_args);
    (void)vrt_frame_set_raise_target(root->frame_id.raw());
    const uint64_t raised = 7;
    vrt_frame_raise(scalar_type_id, &raised);
  }

  uint64_t raised = 0;
  vrt_frame_take_raised_value(scalar_type_id, &raised);
  if (
    (vrt_thread_current_frame() != root) || (raised != 7) ||
    (finalized_sum != 66))
    return 6;

  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != root) ||
    (vrt_frame_func(root) != &tail_function) || (finalized_sum != 77))
    return 7;

  Fields reused_args{33};
  auto* reused_data = vrt_object_stack(&cls, 1, &reused_args);
  if (reused_data != root_data)
    return 8;

  vrt_frame_leave();
  if ((vrt_thread_current_frame() != nullptr) || (finalized_sum != 110))
    return 9;

  vrt_thread_deinit();
  return 0;
}