#include <cstdint>
#include <type_traits>
#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/function.h>
#include <vrt/program.h>
#include <vrt/region.h>
#include <vrt/thread.h>

static_assert(std::is_same_v<vrt_func, vrt::Func>);
static_assert(std::is_same_v<vrt_func_ptr, vrt::FuncPtr>);
static_assert(std::is_same_v<vrt_frame, vrt::Frame>);
static_assert(std::is_same_v<vrt_thread, vrt::Thread>);
static_assert(std::is_same_v<vrt_region_type, vrt::RegionType>);
static_assert(std::is_same_v<vrt_error, vrt::Error>);
static_assert(std::is_same_v<vrt_error_info, vrt::ErrorInfo>);
static_assert(
  std::is_same_v<vrt_invocation_function, vrt::InvocationFunction>);
static_assert(
  std::is_same_v<std::underlying_type_t<vrt::Error>, std::uint32_t>);
static_assert(VRT_ERROR_BAD_ARRAY_INDEX == vrt::Error::bad_array_index);
static_assert(std::is_same_v<decltype(&set_exit_code), void (*)(std::int32_t)>);
static_assert(
  std::is_same_v<decltype(&vrt_error_message), const char* (*)(vrt::Error)>);
static_assert(std::is_same_v<
              decltype(&vrt_try_invoke),
              int (*)(vrt::InvocationFunction, void*, vrt::ErrorInfo*)>);
static_assert(std::is_same_v<decltype(&vrt_error_raise), void (*)(vrt::Error)>);
static_assert(std::is_same_v<decltype(&verona_program_entry), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_thread_current), vrt_thread* (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_thread_current_frame), vrt_frame* (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_enter),
              vrt_frame* (*)(const vrt_func*)>);
static_assert(std::is_same_v<decltype(&vrt_frame_leave), void (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_reuse),
              void (*)(const vrt_func*)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_get_raise_target),
              std::uint64_t (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_set_raise_target),
              std::uint64_t (*)(std::uint64_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_raise_continuation),
              void* (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_raise), void (*)(std::uint64_t)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_take_raised_value),
              std::uint64_t (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_parent), vrt_frame* (*)(vrt_frame*)>);
static_assert(
  std::is_same_v<decltype(&vrt_frame_id), std::uint64_t (*)(const vrt_frame*)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_func),
              const vrt_func* (*)(const vrt_frame*)>);
static_assert(std::is_same_v<
              decltype(&vrt_func_get_ptr),
              vrt_func_ptr (*)(const vrt_func*)>);

extern "C" void verona_program_entry(void)
{
  set_exit_code(0);
}
