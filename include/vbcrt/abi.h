#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  define VBCRT_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#  define VBCRT_EXPORT __attribute__((visibility("default")))
#else
#  define VBCRT_EXPORT
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Set the process exit code.
   *
   * This function is implemented by libvbcrt and may be called by generated
   * Verona code.
   */
  VBCRT_EXPORT void set_exit_code(int32_t code);

  /**
   * Enter the generated Verona program.
   *
   * This function is implemented by generated code and called by libvbcrt.
   */
  VBCRT_EXPORT void verona_main(void);

#if defined(__cplusplus)
}
#endif
