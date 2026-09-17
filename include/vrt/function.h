#pragma once

#include "export.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** Type-erased native entry point for a generated Verona function. */
  using FuncPtr = void (*)(void);

  /** Static metadata describing a generated Verona function. */
  struct Func
  {
    uint64_t id;
    const char* name;
    FuncPtr entry;
  };
}

using vrt_func_ptr = vrt::FuncPtr;
using vrt_func = vrt::Func;
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

  /** Return the native entry point carried by callable metadata. */
  VRT_EXPORT vrt_func_ptr vrt_func_get_ptr(const vrt_func* func);

#if defined(__cplusplus)
}
#endif
