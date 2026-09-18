#include "frame.h"

#include "object.h"
#include "region.h"
#include "region_arena.h"
#include "region_rc.h"
#include "thread.h"
#include "vrt.h"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vrt/program.h>

static_assert(sizeof(vrt::Location) == sizeof(uintptr_t));
static_assert(std::is_trivially_copyable_v<vrt::Location>);
static_assert(!std::is_default_constructible_v<vrt::Location>);
static_assert(
  std::is_same_v<decltype(vrt::Frame::raise_target), vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Frame::frame_id), vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Thread::frame), vrt::Frame*>);
static_assert(sizeof(vrt::Thread) == sizeof(vrt::Frame*));
static_assert(alignof(vrt::Region) > vrt::Location::Mask);
static_assert(alignof(vrt::Header) > vrt::Location::Mask);

namespace
{
  constexpr uintptr_t value_class_id = 0x101;
  constexpr uintptr_t holder_class_id = 0x102;

  struct ValuePayload
  {
    uint64_t value;
  };

  struct HolderPayload
  {
    void* value;
    uint32_t tag;
  };

  const vrt::Field value_fields[] = {
    {offsetof(ValuePayload, value),
     sizeof(ValuePayload::value),
     0,
    vrt::ValueType::scalar}};

  const vrt::Field holder_fields[] = {
    {offsetof(HolderPayload, value),
     sizeof(HolderPayload::value),
     value_class_id,
    vrt::ValueType::object},
    {offsetof(HolderPayload, tag),
     sizeof(HolderPayload::tag),
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

  vrt::Class holder_class{
    holder_class_id,
    "Holder",
    sizeof(HolderPayload),
    alignof(HolderPayload),
    2,
    holder_fields,
    0,
    nullptr,
    nullptr};

  const vrt::TypeInfo types[] = {
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {holder_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Program program{2, types, 0, nullptr};

  vrt::Object* object_from_payload(void* payload)
  {
    return static_cast<vrt::Object*>(
      vrt::header_from_payload(vrt::ValueType::object, payload));
  }

}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);

  auto root_location = vrt::Location::stack();
  auto child_location = root_location.next_stack_level();
  if (
    (root_location.raw() != 0x1) || (child_location.raw() != 0x9) ||
    !root_location.is_stack() || root_location.is_region() ||
    (root_location.stack_index() != 0) || (child_location.stack_index() != 1) ||
    !(root_location < child_location) || !(root_location <= child_location) ||
    !(child_location > root_location) || !(child_location >= root_location) ||
    (root_location == child_location) ||
    (vrt::Location::from_raw(child_location.raw()) != child_location) ||
    !vrt::Location::immortal().is_immortal())
    return 15;

  const vrt::Func root_function{1, "root", nullptr};
  const vrt::Func child_function{2, "child", nullptr};
  const vrt::Func intermediate_function{3, "intermediate", nullptr};
  vrt::init_thread();
  auto* root_frame = vrt_frame_enter(&root_function);
  if (
    (root_frame == nullptr) || (root_frame->region == nullptr) ||
    (root_frame->frame_id != root_location) ||
    !root_frame->region->is_frame_local() ||
    (root_frame->region->frame_depth != 1) ||
    (root_frame->region->type != vrt::RegionType::rc) ||
    (dynamic_cast<vrt::RegionArena*>(root_frame->region) != nullptr) ||
    (dynamic_cast<vrt::RegionRC*>(root_frame->region) == nullptr) ||
    (root_frame->region->header_count() != 0))
    return 1;

  auto* frame_region = root_frame->region;

  // Escaping a return value relocates it from the callee's frame-local
  // region into the caller's region before callee teardown.
  auto* child_frame = vrt_frame_enter(&child_function);
  ValuePayload escaped_args{12};
  auto* escaped = vrt_object_new(&value_class, 1, &escaped_args);
  auto* escaped_object = object_from_payload(escaped);
  auto* callee_region = child_frame->region;
  auto child_frame_id = vrt_frame_id(child_frame);
  if (
    (callee_region == nullptr) || (callee_region == frame_region) ||
    (child_frame->frame_id != child_location) ||
    (callee_region->frame_depth != 2) ||
    (escaped_object->region() != callee_region) ||
    !callee_region->contains(escaped_object))
    return 2;

  auto region_location = escaped_object->location();
  if (
    !region_location.is_region() ||
    (region_location.to_region() != callee_region) ||
    (region_location.raw() != reinterpret_cast<uintptr_t>(callee_region)))
    return 17;

  vrt_object_escape(escaped);
  if (
    (escaped_object->location() != vrt::Location(frame_region)) ||
    !frame_region->contains(escaped_object) ||
    callee_region->contains(escaped_object) ||
    (callee_region->header_count() != 0))
    return 3;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (static_cast<ValuePayload*>(escaped)->value != 12) ||
    (escaped_object->region() != frame_region) ||
    !frame_region->contains(escaped_object))
    return 4;

  vrt_object_release(escaped);
  if (frame_region->header_count() != 0)
    return 5;

  // Preparing an object-valued raise relocates an intermediate frame's
  // object directly into the configured target frame before both intervening
  // frames are unwound.
  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 6;

  if (setjmp(*continuation) == 0)
  {
    auto* intermediate_frame = vrt_frame_enter(&intermediate_function);
    if (
      (intermediate_frame->frame_id != child_location) ||
      (vrt_frame_id(intermediate_frame) != child_frame_id))
      return 16;

    ValuePayload raised_args{13};
    auto* raised = vrt_object_new(&value_class, 1, &raised_args);
    auto* raised_object = object_from_payload(raised);
    auto* intermediate_region = intermediate_frame->region;
    auto* raise_frame = vrt_frame_enter(&child_function);
    if (
      (vrt_frame_set_raise_target(root_frame->frame_id.raw()) !=
       raise_frame->frame_id.raw()) ||
      (raise_frame->frame_id != child_location.next_stack_level()) ||
      (raised_object->region() != intermediate_region) ||
      !intermediate_region->contains(raised_object))
      return 7;

    vrt_object_prepare_raise(raised);
    if (
      (raised_object->region() != frame_region) ||
      !frame_region->contains(raised_object) ||
      intermediate_region->contains(raised_object) ||
      (intermediate_region->header_count() != 0))
      return 8;

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
    return 9;

  vrt_object_release(raised);
  if (frame_region->header_count() != 0)
    return 10;

  // A tailcall preserves the frame-local region. Leaving the reused frame
  // then drops its fields, including external roots into heap regions.
  ValuePayload cleanup_child_args{15};
  auto* cleanup_child = vrt_object_region(
    vrt::RegionType::rc, &value_class, 1, &cleanup_child_args);
  auto* cleanup_child_object = object_from_payload(cleanup_child);
  auto* cleanup_child_region = cleanup_child_object->region();
  vrt_object_retain(cleanup_child);
  HolderPayload frame_cleanup_args{cleanup_child, 18};
  auto* frame_cleanup = vrt_object_new(&holder_class, 2, &frame_cleanup_args);
  if (
    (object_from_payload(frame_cleanup)->region() != frame_region) ||
    (frame_region->header_count() != 1) ||
    (cleanup_child_region->stack_reference_count != 2))
    return 11;

  vrt_frame_reuse(&child_function);
  if (
    (vrt_thread_current_frame() != root_frame) ||
    (root_frame->region != frame_region) ||
    (root_frame->frame_id != root_location) ||
    (frame_region->header_count() != 1) ||
    (cleanup_child_region->stack_reference_count != 2))
    return 12;

  vrt_frame_leave();
  if (
    (vrt_thread_current_frame() != nullptr) ||
    (cleanup_child_region->stack_reference_count != 1) ||
    cleanup_child_region->has_parent() ||
    (static_cast<ValuePayload*>(cleanup_child)->value != 15))
    return 13;

  vrt_object_release(cleanup_child);
  vrt::deinit_thread();
  if (vrt_thread_current() != nullptr)
    return 14;

  return 0;
}
