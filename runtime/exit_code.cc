#include "vbcrt.h"

#include <vbcrt/abi.h>

#include <atomic>

namespace
{
  std::atomic<int32_t> exit_code{0};
}

namespace vbcrt
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

extern "C" VBCRT_EXPORT void set_exit_code(int32_t code)
{
  vbcrt::set_exit_code(code);
}
