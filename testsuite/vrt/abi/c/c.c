// Coverage: C11 ABI sizes, enum values, type identities, descriptor layouts,
// exported function signatures, and linkage for every public VRT header.
// Non-goals: runtime semantics belong to the API, behavior, and internal
// fixtures.

#include <vrt/array.h>
#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/function.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/reference.h>
#include <vrt/region.h>
#include <vrt/thread.h>

_Static_assert(sizeof(vrt_region_type) == sizeof(uint8_t), "region type ABI");
_Static_assert(sizeof(vrt_error) == sizeof(uint32_t), "error ABI");
_Static_assert(VRT_ERROR_NONE == 0, "no error ABI");
_Static_assert(VRT_ERROR_BAD_RAISE_TARGET == 1, "raise error ABI");
_Static_assert(VRT_ERROR_BAD_ALLOC_TARGET == 2, "allocation error ABI");
_Static_assert(VRT_ERROR_BAD_ARRAY_INDEX == 3, "array index error ABI");
_Static_assert(VRT_ERROR_BAD_STORE_TARGET == 4, "store target error ABI");
_Static_assert(VRT_ERROR_BAD_STORE == 5, "store error ABI");
_Static_assert(VRT_ERROR_METHOD_NOT_FOUND == 6, "method error ABI");
_Static_assert(VRT_ERROR_BAD_STACK_ESCAPE == 7, "stack escape error ABI");
_Static_assert(VRT_ERROR_BAD_REGION_ENTRY_POINT == 8, "region entry error ABI");
_Static_assert(VRT_ERROR_BAD_FREEZE == 9, "freeze error ABI");
_Static_assert(VRT_ERROR_BAD_MERGE == 10, "merge error ABI");
_Static_assert(
  VRT_ERROR_SCHEDULER_ALREADY_RUNNING == 11, "scheduler error ABI");
_Static_assert(
  VRT_ERROR_BAD_REFERENCE_TARGET == 12, "reference target error ABI");
_Static_assert(VRT_REGION_RC == 0, "RC region ABI");
_Static_assert(VRT_REGION_ARENA == 1, "arena region ABI");
_Static_assert(VRT_VALUE_TYPE_NONE == 0, "none value type ABI");
_Static_assert(VRT_VALUE_TYPE_SCALAR == 1, "scalar value type ABI");
_Static_assert(VRT_VALUE_TYPE_RAW_POINTER == 2, "pointer value type ABI");
_Static_assert(VRT_VALUE_TYPE_OBJECT == 3, "object value type ABI");
_Static_assert(VRT_VALUE_TYPE_ARRAY == 4, "array value type ABI");
_Static_assert(VRT_VALUE_TYPE_REFERENCE == 5, "reference value type ABI");
_Static_assert(VRT_VALUE_TYPE_COWN == 6, "cown value type ABI");
_Static_assert(VRT_VALUE_TYPE_DYNAMIC == 7, "dynamic value type ABI");
_Static_assert(VRT_VALUE_TYPE_AGGREGATE == 8, "aggregate value type ABI");
_Static_assert(VRT_REFERENCE_REGISTER == 0, "register reference ABI");
_Static_assert(VRT_REFERENCE_FIELD == 1, "field reference ABI");
_Static_assert(VRT_REFERENCE_ARRAY == 2, "array reference ABI");

static const vrt_value_type object_value_type = VRT_VALUE_TYPE_OBJECT;
static const vrt_error bad_array_index = VRT_ERROR_BAD_ARRAY_INDEX;

#define VRT_TYPE_IS(expression, type) \
  _Generic((expression), type: 1, default: 0)

_Static_assert(
  VRT_TYPE_IS((vrt_region_type){0}, uint8_t), "region type identity ABI");
_Static_assert(VRT_TYPE_IS(((vrt_func*)0)->id, uint64_t), "function id ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_func*)0)->name, const char*), "function name ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_func*)0)->entry, vrt_func_ptr), "function pointer ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_error_info*)0)->code, vrt_error), "error code ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_error_info*)0)->func, const vrt_func*),
  "error function ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_error_info*)0)->site, uintptr_t), "error site ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_field*)0)->offset, uintptr_t), "field offset ABI");
_Static_assert(VRT_TYPE_IS(((vrt_field*)0)->size, uintptr_t), "field size ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_field*)0)->type_id, uintptr_t), "field type ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_field*)0)->value_type, uintptr_t), "field value type ABI");
_Static_assert(VRT_TYPE_IS(((vrt_field*)0)->id, uintptr_t), "field id ABI");
_Static_assert(VRT_TYPE_IS(((vrt_method*)0)->id, uintptr_t), "method id ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_method*)0)->func, const vrt_func*), "method function ABI");
_Static_assert(VRT_TYPE_IS(((vrt_class*)0)->id, uintptr_t), "class id ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->name, const char*), "class name ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->data_size, uintptr_t), "class data size ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->data_alignment, uintptr_t),
  "class data alignment ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->field_count, uintptr_t),
  "class field count ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->fields, const vrt_field*), "class fields ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->method_count, uintptr_t),
  "class method count ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->methods, const vrt_method*),
  "class methods ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->singleton, void*), "class singleton ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_class*)0)->finalizer_thunk, vrt_finalizer_thunk),
  "class finalizer thunk ABI");
_Static_assert(VRT_TYPE_IS(((vrt_type*)0)->id, uintptr_t), "type id ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_type*)0)->value_type, uintptr_t), "type value type ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_type*)0)->storage_size, uintptr_t),
  "type storage size ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_type*)0)->element_type_id, uintptr_t),
  "array element type ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_singleton*)0)->storage, void*), "singleton storage ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_singleton*)0)->cls, const vrt_class*),
  "singleton class ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_program*)0)->type_count, uintptr_t),
  "program type count ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_program*)0)->types, const vrt_type*), "program types ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_program*)0)->singleton_count, uintptr_t),
  "program singleton count ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_program*)0)->singletons, const vrt_singleton*),
  "program singletons ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_reference*)0)->kind_flags, uintptr_t),
  "reference kind ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_reference*)0)->owner, void*), "reference owner ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_reference*)0)->target, void*), "reference target ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_reference*)0)->content_type_id, uintptr_t),
  "reference content type ABI");
_Static_assert(
  VRT_TYPE_IS(((vrt_reference*)0)->storage_epoch, uintptr_t),
  "reference storage epoch ABI");

#undef VRT_TYPE_IS

static void (*const set_exit_code_signature)(int32_t) = set_exit_code;
static void (*const runtime_init_signature)(void) = vrt_runtime_init;
static void (*const program_init_signature)(const vrt_program*) =
  vrt_program_init;
static void (*const invocation_begin_signature)(void) = vrt_invocation_begin;
static const char* (*const error_message_signature)(vrt_error) =
  vrt_error_message;
static int (*const try_invoke_signature)(
  vrt_invocation_function, void*, vrt_error_info*) = vrt_try_invoke;
static void (*const error_raise_signature)(vrt_error) = vrt_error_raise;
static void (*const program_entry_signature)(void) = verona_program_entry;
static vrt_thread* (*const thread_current_signature)(void) = vrt_thread_current;
static void (*const thread_init_signature)(void) = vrt_thread_init;
static void (*const thread_deinit_signature)(void) = vrt_thread_deinit;
static vrt_frame* (*const thread_current_frame_signature)(void) =
  vrt_thread_current_frame;
static vrt_frame* (*const frame_enter_signature)(const vrt_func*) =
  vrt_frame_enter;
static void (*const frame_leave_signature)(void) = vrt_frame_leave;
static void (*const frame_reuse_signature)(const vrt_func*) = vrt_frame_reuse;
static uint64_t (*const frame_get_raise_target_signature)(void) =
  vrt_frame_get_raise_target;
static uint64_t (*const frame_set_raise_target_signature)(uint64_t) =
  vrt_frame_set_raise_target;
static void* (*const frame_raise_continuation_signature)(void) =
  vrt_frame_raise_continuation;
static void (*const frame_raise_signature)(uintptr_t, const void*) =
  vrt_frame_raise;
static void (*const frame_take_raised_value_signature)(uintptr_t, void*) =
  vrt_frame_take_raised_value;
static vrt_frame* (*const frame_parent_signature)(vrt_frame*) =
  vrt_frame_parent;
static uint64_t (*const frame_id_signature)(const vrt_frame*) = vrt_frame_id;
static const vrt_func* (*const frame_func_signature)(const vrt_frame*) =
  vrt_frame_func;
static void (*const reference_from_register_signature)(
  vrt_reference*, vrt_frame*, void*, uintptr_t) =
  vrt_reference_from_register;
static void (*const reference_from_field_signature)(
  vrt_reference*, void*, uintptr_t) = vrt_reference_from_field;
static void (*const reference_from_array_signature)(
  vrt_reference*, void*, uintptr_t) = vrt_reference_from_array;
static void (*const reference_retain_signature)(const vrt_reference*) =
  vrt_reference_retain;
static void (*const reference_release_signature)(const vrt_reference*) =
  vrt_reference_release;
static void (*const reference_escape_signature)(const vrt_reference*) =
  vrt_reference_escape;
static void (*const reference_validate_tailcall_signature)(
  const vrt_reference*) = vrt_reference_validate_tailcall;
static void (*const reference_load_signature)(
  const vrt_reference*, void*) = vrt_reference_load;
static void (*const reference_exchange_signature)(
  const vrt_reference*, const void*, void*) = vrt_reference_exchange;
static vrt_func_ptr (*const func_entry_signature)(const vrt_func*) =
  vrt_func_entry;
static void* (*const array_new_signature)(uintptr_t, uintptr_t) = vrt_array_new;
static void* (*const array_stack_signature)(uintptr_t, uintptr_t) =
  vrt_array_stack;
static void* (*const array_heap_signature)(const void*, uintptr_t, uintptr_t) =
  vrt_array_heap;
static void* (*const array_region_signature)(
  vrt_region_type, uintptr_t, uintptr_t) = vrt_array_region;
static void (*const array_retain_signature)(void*) = vrt_array_retain;
static void (*const array_release_signature)(void*) = vrt_array_release;
static void (*const array_freeze_signature)(void*) = vrt_array_freeze;
static void (*const array_escape_signature)(void*) = vrt_array_escape;
static void (*const array_validate_tailcall_signature)(void*) =
  vrt_array_validate_tailcall;
static void (*const array_copy_signature)(
  void*, uintptr_t, void*, uintptr_t, uintptr_t) = vrt_array_copy;
static void (*const array_fill_signature)(
  void*, uintptr_t, uintptr_t, const void*) = vrt_array_fill;
static int64_t (*const array_compare_signature)(
  void*, uintptr_t, void*, uintptr_t, uintptr_t) = vrt_array_compare;
static void* (*const object_new_signature)(
  const vrt_class*, uintptr_t, const void*) = vrt_object_new;
static void* (*const object_stack_signature)(
  const vrt_class*, uintptr_t, const void*) = vrt_object_stack;
static void* (*const object_heap_signature)(
  const void*, const vrt_class*, uintptr_t, const void*) = vrt_object_heap;
static void* (*const object_region_signature)(
  vrt_region_type,
  const vrt_class*,
  uintptr_t,
  const void*) = vrt_object_region;
static uintptr_t (*const object_class_id_signature)(const void*) =
  vrt_object_class_id;
static const vrt_func* (*const object_lookup_signature)(
  const void*, uintptr_t) = vrt_object_lookup;
static void (*const object_retain_signature)(void*) = vrt_object_retain;
static void (*const object_release_signature)(void*) = vrt_object_release;
static void (*const object_freeze_signature)(void*) = vrt_object_freeze;
static void (*const object_escape_signature)(void*) = vrt_object_escape;
static void (*const object_validate_tailcall_signature)(void*) =
  vrt_object_validate_tailcall;

void verona_program_entry(void)
{
  set_exit_code_signature(0);
  (void)runtime_init_signature;
  (void)program_init_signature;
  (void)invocation_begin_signature;
  (void)program_entry_signature;
  (void)error_message_signature;
  (void)try_invoke_signature;
  (void)error_raise_signature;
  (void)thread_current_signature;
  (void)thread_init_signature;
  (void)thread_deinit_signature;
  (void)thread_current_frame_signature;
  (void)frame_enter_signature;
  (void)frame_leave_signature;
  (void)frame_reuse_signature;
  (void)frame_get_raise_target_signature;
  (void)frame_set_raise_target_signature;
  (void)frame_raise_continuation_signature;
  (void)frame_raise_signature;
  (void)frame_take_raised_value_signature;
  (void)frame_parent_signature;
  (void)frame_id_signature;
  (void)frame_func_signature;
  (void)reference_from_register_signature;
  (void)reference_from_field_signature;
  (void)reference_from_array_signature;
  (void)reference_retain_signature;
  (void)reference_release_signature;
  (void)reference_escape_signature;
  (void)reference_validate_tailcall_signature;
  (void)reference_load_signature;
  (void)reference_exchange_signature;
  (void)func_entry_signature;
  (void)array_new_signature;
  (void)array_stack_signature;
  (void)array_heap_signature;
  (void)array_region_signature;
  (void)array_retain_signature;
  (void)array_release_signature;
  (void)array_freeze_signature;
  (void)array_escape_signature;
  (void)array_validate_tailcall_signature;
  (void)array_copy_signature;
  (void)array_fill_signature;
  (void)array_compare_signature;
  (void)object_new_signature;
  (void)object_stack_signature;
  (void)object_heap_signature;
  (void)object_region_signature;
  (void)object_class_id_signature;
  (void)object_lookup_signature;
  (void)object_retain_signature;
  (void)object_release_signature;
  (void)object_freeze_signature;
  (void)object_escape_signature;
  (void)object_validate_tailcall_signature;
  (void)object_value_type;
  (void)bad_array_index;
}

const vrt_program verona_program = {0, 0, 0, 0};
