#pragma once

#include "export.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Set the process exit code.
   *
   * This function is implemented by libvrt and may be called by generated
   * Verona code.
   */
  VRT_EXPORT void set_exit_code(int32_t code);

  /**
   * Enter the generated Verona program.
   *
   * This function is implemented by generated native code and called by
   * libvrt.
   */
  VRT_EXPORT void verona_program_entry(void);

#if defined(__cplusplus)
}
#endif
