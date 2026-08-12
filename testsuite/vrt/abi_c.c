#include <vrt/abi.h>

static void (*const set_exit_code_signature)(int32_t) = set_exit_code;
static void (*const verona_main_signature)(void) = verona_main;
static vrt_thread* (*const thread_create_signature)(void) = vrt_thread_create;
static void (*const thread_destroy_signature)(vrt_thread*) = vrt_thread_destroy;
static vrt_frame* (*const thread_current_frame_signature)(vrt_thread*) =
  vrt_thread_current_frame;
static vrt_frame* (*const frame_enter_signature)(
  vrt_thread*, const vrt_function_descriptor*) = vrt_frame_enter;
static void (*const frame_leave_signature)(vrt_thread*, vrt_frame*) =
  vrt_frame_leave;
static void (*const frame_prepare_tailcall_signature)(
  vrt_thread*,
  vrt_frame*,
  const vrt_function_descriptor*) = vrt_frame_prepare_tailcall;
static vrt_frame* (*const frame_parent_signature)(vrt_frame*) =
  vrt_frame_parent;
static uint64_t (*const frame_id_signature)(const vrt_frame*) = vrt_frame_id;
static const vrt_function_descriptor* (*const frame_function_signature)(
  const vrt_frame*) = vrt_frame_function;

void verona_main(void)
{
  set_exit_code_signature(0);
  (void)verona_main_signature;
  (void)thread_create_signature;
  (void)thread_destroy_signature;
  (void)thread_current_frame_signature;
  (void)frame_enter_signature;
  (void)frame_leave_signature;
  (void)frame_prepare_tailcall_signature;
  (void)frame_parent_signature;
  (void)frame_id_signature;
  (void)frame_function_signature;
}
