// See README.md for the allocation, ownership, and runtime boundaries covered.

#include "array.h"

#include "frame.h"
#include "object.h"
#include "program.h"
#include "region.h"
#include "region_rc.h"
#include "value.h"
#include "writebarrier.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vrt/array.h>
#include <vrt/program.h>
#include <vrt/thread.h>

static_assert(std::is_base_of_v<vrt::Header, vrt::Array>);

namespace
{
  constexpr uintptr_t scalar_type_id = 0x100;
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t scalar_array_type_id = 0x201;
  constexpr uintptr_t object_array_type_id = 0x202;
  constexpr uintptr_t array_array_type_id = 0x203;

  struct alignas(16) ValuePayload
  {
    uint64_t value;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValuePayload, value),
     sizeof(ValuePayload::value),
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

  const vrt::TypeInfo types[] = {
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint32_t), 0},
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {scalar_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_type_id},
    {object_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     value_class_id},
    {array_array_type_id,
     vrt::ValueType::array,
     sizeof(void*),
     scalar_array_type_id}};
  const vrt::Program program{5, types, 0, nullptr};

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

  auto scalar_layout = vrt::layout_type_id(scalar_type_id);
  if (
    (scalar_layout.value_type != vrt::ValueType::scalar) ||
    (scalar_layout.storage_size != sizeof(uint32_t)) ||
    (vrt::unarray(scalar_array_type_id) != scalar_type_id))
    return 1;

  const vrt::Func root_function{1, "root", nullptr};
  const vrt::Func child_function{2, "child", nullptr};
  const vrt::Func intermediate_function{3, "intermediate", nullptr};
  vrt_thread_init();
  auto* root_frame = vrt_frame_enter(&root_function);
  if (
    (root_frame == nullptr) || (root_frame->region == nullptr) ||
    !root_frame->region->is_frame_local() ||
    (dynamic_cast<vrt::RegionRC*>(root_frame->region) == nullptr) ||
    (root_frame->region->header_count() != 0))
    return 2;

  auto* frame_region = root_frame->region;

  auto* frame_payload = vrt_array_new(scalar_array_type_id, 2);
  auto* frame_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, frame_payload}.header());
  if (
    (frame_array->get_size() != 2) ||
    (frame_array->get_stride() != sizeof(uint32_t)) ||
    (frame_array->reference_count != 1) ||
    (frame_region->header_count() != 1))
    return 3;

  vrt_array_retain(frame_payload);
  vrt_array_release(frame_payload);
  if (frame_array->reference_count != 1)
    return 4;

  vrt_array_release(frame_payload);
  if (frame_region->header_count() != 0)
    return 5;

  ValuePayload locator_args{17};
  auto* locator_payload = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &locator_args);
  auto* locator = object_from_payload(locator_payload);
  auto* heap_payload =
    vrt_array_heap(locator_payload, scalar_array_type_id, 3);
  auto* heap_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, heap_payload}.header());
  if (
    (heap_array->region() != locator->region()) ||
    (heap_array->get_size() != 3) ||
    (heap_array->get_stride() != sizeof(uint32_t)) ||
    (locator->region()->header_count() != 2))
    return 6;

  vrt_array_release(heap_payload);
  if (locator->region()->header_count() != 1)
    return 7;
  vrt_object_release(locator_payload);

  auto* region_payload =
    vrt_array_region(vrt::RegionType::rc, scalar_array_type_id, 5);
  auto* region_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, region_payload}.header());
  if (
    (region_array->region() == nullptr) ||
    region_array->region()->is_frame_local() ||
    (dynamic_cast<vrt::RegionRC*>(region_array->region()) == nullptr) ||
    (region_array->get_size() != 5) ||
    (region_array->region()->header_count() != 1))
    return 8;
  vrt_array_release(region_payload);

  auto* scalar_array = frame_region->array(scalar_array_type_id, 4);
  auto* scalar_payload = scalar_array->get_payload();
  auto* scalar_values = static_cast<uint32_t*>(scalar_payload);
  if (
    (scalar_array->value_type() != vrt::ValueType::array) ||
    (scalar_array->location() != vrt::Location(frame_region)) ||
    (scalar_array->get_type_id() != scalar_array_type_id) ||
    (scalar_array->content_type_id() != scalar_type_id) ||
    (scalar_array->get_size() != 4) ||
    (scalar_array->get_stride() != sizeof(uint32_t)) ||
    (scalar_array->get_value_type() != vrt::ValueType::scalar) ||
    (scalar_array->allocation_size_bytes() !=
     vrt::Array::size_of(4, sizeof(uint32_t))) ||
    (scalar_values[0] != 0) || (scalar_values[1] != 0) ||
    (scalar_values[2] != 0) || (scalar_values[3] != 0) ||
    (scalar_array->load(2) != &scalar_values[2]) ||
    (vrt::payload_from_header(scalar_array) != scalar_payload) ||
    !frame_region->contains(scalar_array))
    return 9;
  scalar_array->reg_dec();

  auto* object_array = frame_region->array(object_array_type_id, 3);
  ValuePayload array_value_args{41};
  auto* array_value = vrt_object_new(&value_class, 1, &array_value_args);
  auto* array_value_header = object_from_payload(array_value);
  const vrt::Field object_element{
    0, sizeof(void*), value_class_id, vrt::ValueType::object};
  vrt::writebarrier::init(
    frame_region, object_array->load(0), object_element, &array_value);
  vrt::writebarrier::copy(
    frame_region,
    object_array->load(1),
    object_element,
    object_array->load(0));
  vrt::writebarrier::copy(
    frame_region,
    object_array->load(2),
    object_element,
    object_array->load(0));

  uintptr_t traced = 0;
  object_array->trace_fn([&](vrt::Header* element) {
    if (element == array_value_header)
      traced++;
  });
  if (
    (traced != 3) || (array_value_header->reference_count != 3) ||
    (frame_region->header_count() != 2))
    return 10;

  object_array->reg_dec();
  if (frame_region->header_count() != 0)
    return 11;

  auto* nested_child = frame_region->array(scalar_array_type_id, 1);
  auto* nested_parent = frame_region->array(array_array_type_id, 1);
  auto* nested_child_payload = nested_child->get_payload();
  const vrt::Field nested_element{
    0, sizeof(void*), scalar_array_type_id, vrt::ValueType::array};
  vrt::writebarrier::init(
    frame_region,
    nested_parent->load(0),
    nested_element,
    &nested_child_payload);

  vrt::Header* nested_trace = nullptr;
  nested_parent->trace_fn(
    [&](vrt::Header* element) { nested_trace = element; });
  if (
    (nested_trace != nested_child) ||
    (vrt::Value{vrt::ValueType::array, nested_child_payload}.header() !=
     nested_child) ||
    (frame_region->header_count() != 2))
    return 12;

  nested_parent->reg_dec();
  if (frame_region->header_count() != 0)
    return 13;

  auto* array_frame = vrt_frame_enter(&child_function);
  auto* array_frame_region = array_frame->region;
  auto* dragged_payload = vrt_array_new(object_array_type_id, 1);
  auto* dragged_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, dragged_payload}.header());
  ValuePayload dragged_value_args{43};
  auto* dragged_value =
    vrt_object_new(&value_class, 1, &dragged_value_args);
  auto* dragged_value_header = object_from_payload(dragged_value);
  vrt::writebarrier::init(
    array_frame_region,
    dragged_array->load(0),
    object_element,
    &dragged_value);
  vrt_array_escape(dragged_payload);

  if (
    (dragged_array->location() != vrt::Location(frame_region)) ||
    (dragged_value_header->location() != vrt::Location(frame_region)) ||
    (array_frame_region->header_count() != 0) ||
    !frame_region->contains(dragged_array) ||
    !frame_region->contains(dragged_value_header))
    return 14;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (static_cast<ValuePayload*>(dragged_value)->value != 43))
    return 15;

  vrt_array_release(dragged_payload);
  if (frame_region->header_count() != 0)
    return 16;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 17;

  if (setjmp(*continuation) == 0)
  {
    auto* intermediate_frame = vrt_frame_enter(&intermediate_function);
    auto* intermediate_region = intermediate_frame->region;
    auto* raised_payload = vrt_array_new(scalar_array_type_id, 1);
    auto* raised_array = static_cast<vrt::Array*>(
      vrt::Value{vrt::ValueType::array, raised_payload}.header());
    *static_cast<uint32_t*>(raised_payload) = 13;
    auto* raise_frame = vrt_frame_enter(&child_function);
    if (
      (vrt_frame_set_raise_target(root_frame->frame_id.raw()) !=
       raise_frame->frame_id.raw()) ||
      (raised_array->region() != intermediate_region) ||
      !intermediate_region->contains(raised_array))
      return 18;

    vrt_frame_raise(
      VRT_VALUE_TYPE_ARRAY,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(raised_payload)));
  }

  auto* raised_payload = reinterpret_cast<void*>(
    static_cast<uintptr_t>(vrt_frame_take_raised_value()));
  auto* raised_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, raised_payload}.header());
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (raised_array->region() != frame_region) ||
    (*static_cast<uint32_t*>(raised_payload) != 13) ||
    !frame_region->contains(raised_array))
    return 19;

  vrt_array_release(raised_payload);
  if (frame_region->header_count() != 0)
    return 20;

  vrt_frame_leave();
  vrt_thread_deinit();
  if (vrt_thread_current() != nullptr)
    return 21;

  return 0;
}
