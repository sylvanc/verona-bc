#include "vrt.h"

#include <atomic>
#include <vrt/program.h>

namespace
{
  std::atomic<int32_t> exit_code{0};
}

namespace vrt
{
  void reset_exit_code()
  {
    exit_code = 0;
  }

  void set_exit_code(int32_t code)
  {
    exit_code = code;
  }

  int32_t get_exit_code()
  {
    return exit_code;
  }
}

extern "C" VRT_EXPORT void set_exit_code(int32_t code)
{
  vrt::set_exit_code(code);
}
