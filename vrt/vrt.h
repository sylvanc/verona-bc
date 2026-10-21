#pragma once

#include <cstdint>

namespace vrt
{
  /** Bind a fresh logical Verona thread to the calling native thread. */
  void init_thread();

  /** Destroy the logical Verona thread bound to the calling native thread. */
  void deinit_thread();

  void reset_exit_code();
  void set_exit_code(int32_t code);
  int32_t get_exit_code();
}
