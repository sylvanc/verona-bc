#pragma once

#include "export.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** Type-erased native entry point for a generated Verona function. */
  using FunctionEntry = void (*)(void);

  /** Static metadata describing a generated Verona function. */
  struct Function
  {
    uint64_t id;
    const char* name;
    FunctionEntry entry;
  };
}

using vrt_func_ptr = vrt::FunctionEntry;
using vrt_func = vrt::Function;
#else
/** Type-erased native entry point for a generated Verona function. */
typedef void (*vrt_func_ptr)(void);

/** Static metadata describing a generated Verona function. */
typedef struct vrt_func
{
  uint64_t id;
  const char* name;
  vrt_func_ptr entry;
} vrt_func;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Return the native entry point carried by callable metadata.
   *
   * func and func->entry must be non-null. Invalid callable
   * state terminates the process, matching an infallible dynamic call.
   */
  VRT_EXPORT vrt_func_ptr vrt_func_entry(const vrt_func* func);

#if defined(__cplusplus)
}
#endif
