// See README.md for the array-runtime coverage and test boundaries.

#include "array.h"

#include "frame.h"
#include "object.h"
#include "program.h"
#include "region.h"
#include "region_ext.h"
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

  struct alignas(16) ValueFields
  {
    uint64_t value;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     0,
     vrt::ValueType::scalar}};

  vrt::Class value_class{
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

  auto scalar_layout = vrt::layout_type_id(scalar_type_id);
  if (
    (scalar_layout.value_type != vrt::ValueType::scalar) ||
    (scalar_layout.storage_size != sizeof(uint32_t)) ||
    (vrt::unarray(scalar_array_type_id) != scalar_type_id))
    return 1;

  const vrt::Function root_function{1, "root", nullptr};
  const vrt::Function child_function{2, "child", nullptr};
  const vrt::Function intermediate_function{3, "intermediate", nullptr};
  vrt_thread_init();
  auto* root_frame = vrt_frame_enter(&root_function);
  if (
    (root_frame == nullptr) || (root_frame->region == nullptr) ||
    !root_frame->region->is_frame_local() ||
    (dynamic_cast<vrt::RegionRC*>(root_frame->region) == nullptr) ||
    (root_frame->region->header_count() != 0))
    return 2;

  auto* frame_region = root_frame->region;

  auto* frame_array_elements = vrt_array_new(scalar_array_type_id, 2);
  auto* frame_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, frame_array_elements}.header());
  if (
    (frame_array->get_size() != 2) ||
    (frame_array->get_stride() != sizeof(uint32_t)) ||
    (frame_array->reference_count != 1) || (frame_region->header_count() != 1))
    return 3;

  vrt_array_retain(frame_array_elements);
  vrt_array_release(frame_array_elements);
  if (frame_array->reference_count != 1)
    return 4;

  vrt_array_release(frame_array_elements);
  if (frame_region->header_count() != 0)
    return 5;

  ValueFields locator_object_args{17};
  auto* locator_object_data = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &locator_object_args);
  auto* locator_object = object_from_data(locator_object_data);
  auto* heap_array_elements =
    vrt_array_heap(locator_object_data, scalar_array_type_id, 3);
  auto* heap_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, heap_array_elements}.header());
  if (
    (heap_array->region() != locator_object->region()) ||
    (heap_array->get_size() != 3) ||
    (heap_array->get_stride() != sizeof(uint32_t)) ||
    (locator_object->region()->header_count() != 2))
    return 6;

  vrt_array_release(heap_array_elements);
  if (locator_object->region()->header_count() != 1)
    return 7;
  vrt_object_release(locator_object_data);

  auto* region_array_elements =
    vrt_array_region(vrt::RegionType::rc, scalar_array_type_id, 5);
  auto* region_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, region_array_elements}.header());
  if (
    (region_array->region() == nullptr) ||
    region_array->region()->is_frame_local() ||
    (dynamic_cast<vrt::RegionRC*>(region_array->region()) == nullptr) ||
    (region_array->get_size() != 5) ||
    (region_array->region()->header_count() != 1))
    return 8;
  vrt_array_release(region_array_elements);

  auto* scalar_array = frame_region->array(scalar_array_type_id, 4);
  auto* scalar_array_elements = scalar_array->elements();
  auto* scalar_values = static_cast<uint32_t*>(scalar_array_elements);
  if (
    (scalar_array->value_type() != vrt::ValueType::array) ||
    (scalar_array->location() != vrt::Location(frame_region)) ||
    (scalar_array->get_type_id() != scalar_array_type_id) ||
    (scalar_array->content_type_id() != scalar_type_id) ||
    (scalar_array->get_size() != 4) ||
    (scalar_array->get_stride() != sizeof(uint32_t)) ||
    (scalar_array->get_value_type() != vrt::ValueType::scalar) ||
    !scalar_array->is_primitive() ||
    (scalar_array->allocation_size_bytes() !=
     vrt::Array::size_of(4, sizeof(uint32_t))) ||
    (scalar_values[0] != 0) || (scalar_values[1] != 0) ||
    (scalar_values[2] != 0) || (scalar_values[3] != 0) ||
    (scalar_array->load(2) != &scalar_values[2]) ||
    (scalar_array->data() != scalar_array_elements) ||
    !frame_region->contains(scalar_array))
    return 9;

  uint32_t fill = 7;
  vrt_array_fill(scalar_array_elements, 0, scalar_array->get_size(), &fill);
  if (
    (scalar_values[0] != 7) || (scalar_values[1] != 7) ||
    (scalar_values[2] != 7) || (scalar_values[3] != 7))
    return 22;

  scalar_values[0] = 1;
  scalar_values[1] = 2;
  scalar_values[2] = 3;
  scalar_values[3] = 4;
  vrt_array_copy(scalar_array_elements, 1, scalar_array_elements, 0, 3);
  if (
    (scalar_values[0] != 1) || (scalar_values[1] != 1) ||
    (scalar_values[2] != 2) || (scalar_values[3] != 3))
    return 23;

  auto* scalar_copy_array = frame_region->array(scalar_array_type_id, 4);
  auto* scalar_copy_array_elements = scalar_copy_array->elements();
  vrt_array_copy(scalar_copy_array_elements, 0, scalar_array_elements, 0, 4);
  if (
    vrt_array_compare(
      scalar_copy_array_elements, 0, scalar_array_elements, 0, 4) != 0)
    return 24;

  scalar_copy_array->set_size(3);
  scalar_copy_array->set_size(4);
  if (scalar_copy_array->get_size() != 3)
    return 29;

  // Match VBCI's no-op contract: zero-length bulk operations do not inspect
  // offsets or fill values.
  vrt_array_copy(scalar_array_elements, 99, scalar_copy_array_elements, 99, 0);
  vrt_array_fill(scalar_array_elements, 99, 0, nullptr);
  if (
    vrt_array_compare(
      scalar_array_elements, 99, scalar_copy_array_elements, 99, 0) != 0)
    return 25;

  scalar_array->root_ref_dec();
  scalar_copy_array->root_ref_dec();
  if (frame_region->header_count() != 0)
    return 26;

  auto* object_array = frame_region->array(object_array_type_id, 3);
  uintptr_t empty_traced = 0;
  static_cast<vrt::Header*>(object_array)->trace_fn([&](vrt::Header*) {
    empty_traced++;
  });
  if (empty_traced != 0)
    return 30;

  ValueFields array_value_object_args{41};
  auto* array_value_object_data =
    vrt_object_new(&value_class, 1, &array_value_object_args);
  auto* array_value_object = object_from_data(array_value_object_data);
  const vrt::Field object_element{
    0, sizeof(void*), value_class_id, vrt::ValueType::object};
  vrt::writebarrier::init(
    vrt::Location(frame_region),
    object_array->load(0),
    object_element,
    &array_value_object_data);
  vrt_array_fill(object_array->elements(), 1, 2, object_array->load(0));

  uintptr_t traced = 0;
  static_cast<vrt::Header*>(object_array)->trace_fn([&](vrt::Header* element) {
    if (element == array_value_object)
      traced++;
  });
  if (
    (traced != 3) || (array_value_object->reference_count != 3) ||
    (frame_region->header_count() != 2))
    return 10;

  auto* object_copy_array = frame_region->array(object_array_type_id, 3);
  vrt_array_copy(
    object_copy_array->elements(), 0, object_array->elements(), 0, 3);
  uintptr_t copied = 0;
  object_copy_array->trace_fn([&](vrt::Header* element) {
    if (element == array_value_object)
      copied++;
  });
  if (
    (copied != 3) || (array_value_object->reference_count != 6) ||
    (frame_region->header_count() != 3))
    return 27;

  object_copy_array->root_ref_dec();
  if (
    (array_value_object->reference_count != 3) ||
    (frame_region->header_count() != 2))
    return 28;

  object_array->root_ref_dec();
  if (frame_region->header_count() != 0)
    return 11;

  auto* nested_child_array = frame_region->array(scalar_array_type_id, 1);
  auto* nested_parent_array = frame_region->array(array_array_type_id, 1);
  auto* nested_child_array_elements = nested_child_array->elements();
  const vrt::Field nested_element{
    0, sizeof(void*), scalar_array_type_id, vrt::ValueType::array};
  vrt::writebarrier::init(
    vrt::Location(frame_region),
    nested_parent_array->load(0),
    nested_element,
    &nested_child_array_elements);

  vrt::Header* nested_trace = nullptr;
  nested_parent_array->trace_fn(
    [&](vrt::Header* element) { nested_trace = element; });
  if (
    (nested_trace != nested_child_array) ||
    (vrt::Value{vrt::ValueType::array, nested_child_array_elements}.header() !=
     nested_child_array) ||
    (frame_region->header_count() != 2))
    return 12;

  nested_parent_array->root_ref_dec();
  if (frame_region->header_count() != 0)
    return 13;

  auto* array_frame = vrt_frame_enter(&child_function);
  auto* array_frame_region = array_frame->region;
  auto* dragged_array_elements = vrt_array_new(object_array_type_id, 1);
  auto* dragged_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, dragged_array_elements}.header());
  ValueFields dragged_object_args{43};
  auto* dragged_object_data =
    vrt_object_new(&value_class, 1, &dragged_object_args);
  auto* dragged_object = object_from_data(dragged_object_data);
  vrt::writebarrier::init(
    vrt::Location(array_frame_region),
    dragged_array->load(0),
    object_element,
    &dragged_object_data);
  vrt_array_escape(dragged_array_elements);

  if (
    (dragged_array->location() != vrt::Location(frame_region)) ||
    (dragged_object->location() != vrt::Location(frame_region)) ||
    (array_frame_region->header_count() != 0) ||
    !frame_region->contains(dragged_array) ||
    !frame_region->contains(dragged_object))
    return 14;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (static_cast<ValueFields*>(dragged_object_data)->value != 43))
    return 15;

  vrt_array_release(dragged_array_elements);
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
    auto* raised_array_elements = vrt_array_new(scalar_array_type_id, 1);
    auto* raised_array = static_cast<vrt::Array*>(
      vrt::Value{vrt::ValueType::array, raised_array_elements}.header());
    *static_cast<uint32_t*>(raised_array_elements) = 13;
    auto* raise_frame = vrt_frame_enter(&child_function);
    if (
      (vrt_frame_set_raise_target(root_frame->frame_id.raw()) !=
       raise_frame->frame_id.raw()) ||
      (raised_array->region() != intermediate_region) ||
      !intermediate_region->contains(raised_array))
      return 18;

    vrt_frame_raise(
      VRT_VALUE_TYPE_ARRAY,
      static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(raised_array_elements)));
  }

  auto* raised_array_elements = reinterpret_cast<void*>(
    static_cast<uintptr_t>(vrt_frame_take_raised_value()));
  auto* raised_array = static_cast<vrt::Array*>(
    vrt::Value{vrt::ValueType::array, raised_array_elements}.header());
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (raised_array->region() != frame_region) ||
    (*static_cast<uint32_t*>(raised_array_elements) != 13) ||
    !frame_region->contains(raised_array))
    return 19;

  vrt_array_release(raised_array_elements);
  if (frame_region->header_count() != 0)
    return 20;

  vrt_frame_leave();
  vrt_thread_deinit();
  if (vrt_thread_current() != nullptr)
    return 21;

  return 0;
}
