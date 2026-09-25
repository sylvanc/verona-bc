// Coverage: C++ ABI aliases, enum correspondence, descriptor layouts,
// exported function signatures, and linkage for every public VRT header.
// Non-goals: runtime semantics belong to the API, behavior, and internal
// fixtures.

#include <cstdint>
#include <type_traits>
#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/function.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/reference.h>
#include <vrt/region.h>
#include <vrt/thread.h>

static_assert(std::is_same_v<vrt_frame, vrt::Frame>);
static_assert(std::is_same_v<vrt_thread, vrt::Thread>);
static_assert(std::is_same_v<vrt_region_type, vrt::RegionType>);
static_assert(std::is_same_v<vrt_error, vrt::Error>);
static_assert(std::is_same_v<vrt_error_info, vrt::ErrorInfo>);
static_assert(std::is_same_v<vrt_invocation_function, vrt::InvocationFunction>);
static_assert(
  std::is_same_v<std::underlying_type_t<vrt::Error>, std::uint32_t>);
static_assert(VRT_ERROR_NONE == vrt::Error::none);
static_assert(VRT_ERROR_BAD_RAISE_TARGET == vrt::Error::bad_raise_target);
static_assert(VRT_ERROR_BAD_ALLOC_TARGET == vrt::Error::bad_alloc_target);
static_assert(VRT_ERROR_BAD_ARRAY_INDEX == vrt::Error::bad_array_index);
static_assert(VRT_ERROR_BAD_STORE_TARGET == vrt::Error::bad_store_target);
static_assert(VRT_ERROR_BAD_STORE == vrt::Error::bad_store);
static_assert(VRT_ERROR_METHOD_NOT_FOUND == vrt::Error::method_not_found);
static_assert(VRT_ERROR_BAD_STACK_ESCAPE == vrt::Error::bad_stack_escape);
static_assert(
  VRT_ERROR_BAD_REGION_ENTRY_POINT == vrt::Error::bad_region_entry_point);
static_assert(VRT_ERROR_BAD_FREEZE == vrt::Error::bad_freeze);
static_assert(VRT_ERROR_BAD_MERGE == vrt::Error::bad_merge);
static_assert(
  VRT_ERROR_SCHEDULER_ALREADY_RUNNING == vrt::Error::scheduler_already_running);
static_assert(
  VRT_ERROR_BAD_REFERENCE_TARGET == vrt::Error::bad_reference_target);
static_assert(
  std::is_same_v<std::underlying_type_t<vrt::RegionType>, std::uint8_t>);
static_assert(VRT_REGION_RC == vrt::RegionType::rc);
static_assert(VRT_REGION_ARENA == vrt::RegionType::arena);
static_assert(std::is_same_v<vrt_func_ptr, vrt::FunctionEntry>);
static_assert(std::is_same_v<vrt_func, vrt::Function>);
static_assert(std::is_same_v<vrt_field, vrt::Field>);
static_assert(std::is_same_v<vrt_method, vrt::Method>);
static_assert(std::is_same_v<vrt_class, vrt::Class>);
static_assert(std::is_same_v<vrt_finalizer_thunk, vrt::FinalizerThunk>);
static_assert(std::is_same_v<vrt_type, vrt::TypeInfo>);
static_assert(std::is_same_v<vrt_singleton, vrt::Singleton>);
static_assert(std::is_same_v<vrt_program, vrt::Program>);
static_assert(std::is_same_v<vrt_reference, vrt::Reference>);
static_assert(std::is_same_v<vrt_value_type, vrt::ValueType>);
static_assert(
  std::is_same_v<std::underlying_type_t<vrt::ValueType>, std::uintptr_t>);
static_assert(VRT_VALUE_TYPE_NONE == vrt::ValueType::none);
static_assert(VRT_VALUE_TYPE_SCALAR == vrt::ValueType::scalar);
static_assert(VRT_VALUE_TYPE_RAW_POINTER == vrt::ValueType::raw_pointer);
static_assert(VRT_VALUE_TYPE_OBJECT == vrt::ValueType::object);
static_assert(VRT_VALUE_TYPE_ARRAY == vrt::ValueType::array);
static_assert(VRT_VALUE_TYPE_REFERENCE == vrt::ValueType::reference);
static_assert(VRT_VALUE_TYPE_COWN == vrt::ValueType::cown);
static_assert(VRT_VALUE_TYPE_DYNAMIC == vrt::ValueType::dynamic);
static_assert(VRT_VALUE_TYPE_AGGREGATE == vrt::ValueType::aggregate);

static_assert(std::is_standard_layout_v<vrt::Field>);
static_assert(std::is_standard_layout_v<vrt::Method>);
static_assert(std::is_standard_layout_v<vrt::Class>);
static_assert(std::is_standard_layout_v<vrt::TypeInfo>);
static_assert(std::is_standard_layout_v<vrt::Singleton>);
static_assert(std::is_standard_layout_v<vrt::Program>);
static_assert(std::is_standard_layout_v<vrt::Reference>);
static_assert(std::is_standard_layout_v<vrt::Function>);
static_assert(std::is_standard_layout_v<vrt::ErrorInfo>);
static_assert(std::is_same_v<decltype(vrt::ErrorInfo{}.code), vrt::Error>);
static_assert(
  std::is_same_v<decltype(vrt::ErrorInfo{}.func), const vrt::Function*>);
static_assert(std::is_same_v<decltype(vrt::ErrorInfo{}.site), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Function{}.id), std::uint64_t>);
static_assert(std::is_same_v<decltype(vrt::Function{}.name), const char*>);
static_assert(
  std::is_same_v<decltype(vrt::Function{}.entry), vrt::FunctionEntry>);
static_assert(std::is_same_v<decltype(vrt::Field{}.offset), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Field{}.size), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Field{}.type_id), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Field{}.value_type), vrt::ValueType>);
static_assert(std::is_same_v<decltype(vrt::Field{}.id), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Method{}.id), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Method{}.func), const vrt::Function*>);
static_assert(std::is_same_v<decltype(vrt::Class{}.id), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Class{}.name), const char*>);
static_assert(std::is_same_v<decltype(vrt::Class{}.data_size), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Class{}.data_alignment), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Class{}.field_count), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Class{}.fields), const vrt::Field*>);
static_assert(
  std::is_same_v<decltype(vrt::Class{}.method_count), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Class{}.methods), const vrt::Method*>);
static_assert(std::is_same_v<decltype(vrt::Class{}.singleton), void*>);
static_assert(
  std::is_same_v<decltype(vrt::Class{}.finalizer_thunk), vrt::FinalizerThunk>);
static_assert(std::is_same_v<decltype(vrt::TypeInfo{}.id), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::TypeInfo{}.value_type), vrt::ValueType>);
static_assert(
  std::is_same_v<decltype(vrt::TypeInfo{}.storage_size), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::TypeInfo{}.element_type_id), std::uintptr_t>);
static_assert(std::is_same_v<decltype(vrt::Singleton{}.storage), void*>);
static_assert(
  std::is_same_v<decltype(vrt::Singleton{}.cls), const vrt::Class*>);
static_assert(
  std::is_same_v<decltype(vrt::Program{}.type_count), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Program{}.types), const vrt::TypeInfo*>);
static_assert(
  std::is_same_v<decltype(vrt::Program{}.singleton_count), std::uintptr_t>);
static_assert(
  std::is_same_v<decltype(vrt::Program{}.singletons), const vrt::Singleton*>);

static_assert(std::is_same_v<decltype(&set_exit_code), void (*)(std::int32_t)>);
static_assert(std::is_same_v<decltype(&vrt_runtime_init), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_program_init), void (*)(const vrt_program*)>);
static_assert(std::is_same_v<decltype(&vrt_invocation_begin), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_error_message), const char* (*)(vrt::Error)>);
static_assert(std::is_same_v<
              decltype(&vrt_try_invoke),
              int (*)(vrt::InvocationFunction, void*, vrt::ErrorInfo*)>);
static_assert(std::is_same_v<decltype(&vrt_error_raise), void (*)(vrt::Error)>);
static_assert(std::is_same_v<decltype(&verona_program_entry), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_thread_current), vrt_thread* (*)(void)>);
static_assert(std::is_same_v<decltype(&vrt_thread_init), void (*)(void)>);
static_assert(std::is_same_v<decltype(&vrt_thread_deinit), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_thread_current_frame), vrt_frame* (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_enter), vrt_frame* (*)(const vrt_func*)>);
static_assert(std::is_same_v<decltype(&vrt_frame_leave), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_reuse), void (*)(const vrt_func*)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_get_raise_target),
              std::uint64_t (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_set_raise_target),
              std::uint64_t (*)(std::uint64_t)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_raise_continuation), void* (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_raise),
              void (*)(std::uintptr_t, const void*)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_take_raised_value),
              void (*)(std::uintptr_t, void*)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_parent), vrt_frame* (*)(vrt_frame*)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_id), std::uint64_t (*)(const vrt_frame*)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_func),
              const vrt_func* (*)(const vrt_frame*)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_from_register),
              void (*)(vrt_reference*, vrt_frame*, void*, std::uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_from_field),
              void (*)(vrt_reference*, void*, std::uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_from_array),
              void (*)(vrt_reference*, void*, std::uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_retain),
              void (*)(const vrt_reference*)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_release),
              void (*)(const vrt_reference*)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_escape),
              void (*)(const vrt_reference*)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_validate_tailcall),
              void (*)(const vrt_reference*)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_load),
              void (*)(const vrt_reference*, void*)>);
static_assert(std::is_same_v<
              decltype(&vrt_reference_exchange),
              void (*)(const vrt_reference*, const void*, void*)>);
static_assert(
  std::is_same_v<decltype(&vrt_func_entry), vrt_func_ptr (*)(const vrt_func*)>);
static_assert(std::is_same_v<
              decltype(&vrt_array_new),
              void* (*)(std::uintptr_t, std::uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_array_heap),
              void* (*)(const void*, std::uintptr_t, std::uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_array_region),
              void* (*)(vrt::RegionType, std::uintptr_t, std::uintptr_t)>);
static_assert(std::is_same_v<decltype(&vrt_array_retain), void (*)(void*)>);
static_assert(std::is_same_v<decltype(&vrt_array_release), void (*)(void*)>);
static_assert(std::is_same_v<decltype(&vrt_array_freeze), void (*)(void*)>);
static_assert(std::is_same_v<decltype(&vrt_array_escape), void (*)(void*)>);
static_assert(std::is_same_v<
              decltype(&vrt_array_copy),
              void (*)(void*, uintptr_t, void*, uintptr_t, uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_array_fill),
              void (*)(void*, uintptr_t, uintptr_t, const void*)>);
static_assert(std::is_same_v<
              decltype(&vrt_array_compare),
              int64_t (*)(void*, uintptr_t, void*, uintptr_t, uintptr_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_object_new),
              void* (*)(const vrt_class*, std::uintptr_t, const void*)>);
static_assert(
  std::is_same_v<
    decltype(&vrt_object_heap),
    void* (*)(const void*, const vrt_class*, std::uintptr_t, const void*)>);
static_assert(
  std::is_same_v<
    decltype(&vrt_object_region),
    void* (*)(vrt_region_type, const vrt_class*, std::uintptr_t, const void*)>);
static_assert(std::is_same_v<
              decltype(&vrt_object_class_id),
              std::uintptr_t (*)(const void*)>);
static_assert(std::is_same_v<
              decltype(&vrt_object_lookup),
              const vrt_func* (*)(const void*, std::uintptr_t)>);
static_assert(std::is_same_v<decltype(&vrt_object_retain), void (*)(void*)>);
static_assert(std::is_same_v<decltype(&vrt_object_release), void (*)(void*)>);
static_assert(std::is_same_v<decltype(&vrt_object_freeze), void (*)(void*)>);
static_assert(std::is_same_v<decltype(&vrt_object_escape), void (*)(void*)>);

extern "C" void verona_program_entry(void)
{
  set_exit_code(0);
}

extern "C" const vrt_program verona_program{0, nullptr, 0, nullptr};
