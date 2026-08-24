#include <vrt/frame.h>
#include <vrt/program.h>
#include <vrt/thread.h>

static void (*const set_exit_code_signature)(int32_t) = set_exit_code;
static void (*const program_entry_signature)(void) = verona_program_entry;
static vrt_thread* (*const thread_current_signature)(void) = vrt_thread_current;
static vrt_frame* (*const thread_current_frame_signature)(void) =
  vrt_thread_current_frame;
static vrt_frame* (*const frame_enter_signature)(
  const vrt_function_descriptor*) = vrt_frame_enter;
static void (*const frame_leave_signature)(void) = vrt_frame_leave;
static void (*const frame_prepare_tailcall_signature)(
  const vrt_function_descriptor*) = vrt_frame_prepare_tailcall;
static uint64_t (*const frame_get_raise_target_signature)(void) =
  vrt_frame_get_raise_target;
static uint64_t (*const frame_set_raise_target_signature)(uint64_t) =
  vrt_frame_set_raise_target;
static void* (*const frame_raise_continuation_signature)(vrt_frame*) =
  vrt_frame_raise_continuation;
static void (*const frame_raise_signature)(uint64_t) = vrt_frame_raise;
static uint64_t (*const frame_take_raised_value_signature)(void) =
  vrt_frame_take_raised_value;
static vrt_frame* (*const frame_parent_signature)(vrt_frame*) =
  vrt_frame_parent;
static uint64_t (*const frame_id_signature)(const vrt_frame*) = vrt_frame_id;
static const vrt_function_descriptor* (*const frame_function_signature)(
  const vrt_frame*) = vrt_frame_function;

void verona_program_entry(void)
{
  set_exit_code_signature(0);
  (void)program_entry_signature;
  (void)thread_current_signature;
  (void)thread_current_frame_signature;
  (void)frame_enter_signature;
  (void)frame_leave_signature;
  (void)frame_prepare_tailcall_signature;
  (void)frame_get_raise_target_signature;
  (void)frame_set_raise_target_signature;
  (void)frame_raise_continuation_signature;
  (void)frame_raise_signature;
  (void)frame_take_raised_value_signature;
  (void)frame_parent_signature;
  (void)frame_id_signature;
  (void)frame_function_signature;
}
