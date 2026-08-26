#include <cstdint>
#include <type_traits>
#include <vrt/frame.h>
#include <vrt/program.h>
#include <vrt/thread.h>

static_assert(std::is_same_v<decltype(&set_exit_code), void (*)(std::int32_t)>);
static_assert(std::is_same_v<decltype(&verona_program_entry), void (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_thread_current), vrt_thread* (*)(void)>);
static_assert(
  std::is_same_v<decltype(&vrt_thread_current_frame), vrt_frame* (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_enter),
              vrt_frame* (*)(const vrt_function_descriptor*)>);
static_assert(std::is_same_v<decltype(&vrt_frame_leave), void (*)(void)>);
static_assert(std::is_same_v<
              decltype(&vrt_frame_reuse),
              void (*)(const vrt_function_descriptor*)>);
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
              decltype(&vrt_frame_function),
              const vrt_function_descriptor* (*)(const vrt_frame*)>);

extern "C" void verona_program_entry(void)
{
  set_exit_code(0);
}
