#pragma once

#include <cstdint>

namespace vrt
{
  void reset_exit_code();
  void set_exit_code(int32_t code);
  int32_t get_exit_code();
}
