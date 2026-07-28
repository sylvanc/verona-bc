#include <vrt/abi.h>

#include <cstdint>
#include <type_traits>

static_assert(
  std::is_same_v<decltype(&set_exit_code), void (*)(std::int32_t)>);
static_assert(std::is_same_v<decltype(&verona_main), void (*)(void)>);

extern "C" void verona_main(void)
{
  set_exit_code(0);
}
