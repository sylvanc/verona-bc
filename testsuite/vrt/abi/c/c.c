#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/function.h>
#include <vrt/object.h>
#include <vrt/program.h>
#include <vrt/region.h>
#include <vrt/thread.h>

_Static_assert(sizeof(vrt_error) == sizeof(uint32_t), "error ABI");
_Static_assert(VRT_ERROR_NONE == 0, "no error ABI");
_Static_assert(VRT_ERROR_BAD_RAISE_TARGET == 1, "raise error ABI");
_Static_assert(VRT_ERROR_BAD_ALLOC_TARGET == 2, "allocation error ABI");
_Static_assert(
  sizeof(vrt_field) == (4 * sizeof(uintptr_t)), "field metadata ABI");
_Static_assert(
  sizeof(((vrt_class*)0)->singleton) == sizeof(void*), "class metadata ABI");

static const vrt_error bad_array_index = VRT_ERROR_BAD_ARRAY_INDEX;

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
static vrt_frame* (*const frame_enter_signature)(
  const vrt_func*) = vrt_frame_enter;
static void (*const frame_leave_signature)(void) = vrt_frame_leave;
static void (*const frame_reuse_signature)(const vrt_func*) =
  vrt_frame_reuse;
static uint64_t (*const frame_get_raise_target_signature)(void) =
  vrt_frame_get_raise_target;
static uint64_t (*const frame_set_raise_target_signature)(uint64_t) =
  vrt_frame_set_raise_target;
static void* (*const frame_raise_continuation_signature)(void) =
  vrt_frame_raise_continuation;
static void (*const frame_raise_signature)(uint64_t) = vrt_frame_raise;
static uint64_t (*const frame_take_raised_value_signature)(void) =
  vrt_frame_take_raised_value;
static vrt_frame* (*const frame_parent_signature)(vrt_frame*) =
  vrt_frame_parent;
static uint64_t (*const frame_id_signature)(const vrt_frame*) = vrt_frame_id;
static const vrt_func* (*const frame_func_signature)(const vrt_frame*) =
  vrt_frame_func;
static vrt_func_ptr (*const func_get_ptr_signature)(const vrt_func*) =
  vrt_func_get_ptr;
static void* (*const object_new_signature)(
  const vrt_class*, uintptr_t, const void*) = vrt_object_new;
static void* (*const object_heap_signature)(
  const void*, const vrt_class*, uintptr_t, const void*) = vrt_object_heap;
static void* (*const object_region_signature)(
  vrt_region_type,
  const vrt_class*,
  uintptr_t,
  const void*) = vrt_object_region;
static void (*const object_retain_signature)(void*) = vrt_object_retain;
static void (*const object_release_signature)(void*) = vrt_object_release;
static void (*const object_escape_signature)(void*) = vrt_object_escape;
static void (*const object_prepare_raise_signature)(void*) =
  vrt_object_prepare_raise;

void verona_program_entry(void)
{
  set_exit_code_signature(0);
  (void)runtime_init_signature;
  (void)program_init_signature;
  (void)invocation_begin_signature;
  (void)error_message_signature;
  (void)try_invoke_signature;
  (void)error_raise_signature;
  (void)program_entry_signature;
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
  (void)func_get_ptr_signature;
  (void)object_new_signature;
  (void)object_heap_signature;
  (void)object_region_signature;
  (void)object_retain_signature;
  (void)object_release_signature;
  (void)object_escape_signature;
  (void)object_prepare_raise_signature;
  (void)bad_array_index;
}

const vrt_program verona_program = {0, 0, 0, 0};
