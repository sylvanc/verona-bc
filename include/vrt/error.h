#pragma once

#include "export.h"
#include "function.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** A language/runtime error reported by a failed native invocation. */
  enum class Error : uint32_t
  {
    none = 0,
    bad_raise_target = 1,
    bad_alloc_target = 2,
    bad_array_index = 3,
    bad_store_target = 4,
    bad_store = 5,
    method_not_found = 6,
    bad_stack_escape = 7,
    bad_region_entry_point = 8,
    bad_freeze = 9,
    bad_merge = 10,
    scheduler_already_running = 11,
  };

  /**
   * Metadata describing a language/runtime error raised by generated code.
   *
   * func is borrowed. Compiler-emitted descriptors have program lifetime;
   * callers supplying their own descriptors must keep them alive while this
   * record is inspected. site is zero when no stable site is available.
   */
  struct ErrorInfo
  {
    Error code = Error::none;
    const Func* func = nullptr;
    uintptr_t site = 0;
  };

  using InvocationFunction = void (*)(void*);
}

using vrt_error = vrt::Error;
using vrt_error_info = vrt::ErrorInfo;
using vrt_invocation_function = vrt::InvocationFunction;

inline constexpr auto VRT_ERROR_NONE = vrt::Error::none;
inline constexpr auto VRT_ERROR_BAD_RAISE_TARGET = vrt::Error::bad_raise_target;
inline constexpr auto VRT_ERROR_BAD_ALLOC_TARGET = vrt::Error::bad_alloc_target;
inline constexpr auto VRT_ERROR_BAD_ARRAY_INDEX = vrt::Error::bad_array_index;
inline constexpr auto VRT_ERROR_BAD_STORE_TARGET = vrt::Error::bad_store_target;
inline constexpr auto VRT_ERROR_BAD_STORE = vrt::Error::bad_store;
inline constexpr auto VRT_ERROR_METHOD_NOT_FOUND = vrt::Error::method_not_found;
inline constexpr auto VRT_ERROR_BAD_STACK_ESCAPE = vrt::Error::bad_stack_escape;
inline constexpr auto VRT_ERROR_BAD_REGION_ENTRY_POINT =
  vrt::Error::bad_region_entry_point;
inline constexpr auto VRT_ERROR_BAD_FREEZE = vrt::Error::bad_freeze;
inline constexpr auto VRT_ERROR_BAD_MERGE = vrt::Error::bad_merge;
inline constexpr auto VRT_ERROR_SCHEDULER_ALREADY_RUNNING =
  vrt::Error::scheduler_already_running;
#else
typedef uint32_t vrt_error;
typedef void (*vrt_invocation_function)(void*);

enum
{
  VRT_ERROR_NONE = 0,
  VRT_ERROR_BAD_RAISE_TARGET = 1,
  VRT_ERROR_BAD_ALLOC_TARGET = 2,
  VRT_ERROR_BAD_ARRAY_INDEX = 3,
  VRT_ERROR_BAD_STORE_TARGET = 4,
  VRT_ERROR_BAD_STORE = 5,
  VRT_ERROR_METHOD_NOT_FOUND = 6,
  VRT_ERROR_BAD_STACK_ESCAPE = 7,
  VRT_ERROR_BAD_REGION_ENTRY_POINT = 8,
  VRT_ERROR_BAD_FREEZE = 9,
  VRT_ERROR_BAD_MERGE = 10,
  VRT_ERROR_SCHEDULER_ALREADY_RUNNING = 11
};

/**
 * Metadata describing a language/runtime error raised by generated code.
 *
 * func is borrowed and must remain alive while this record is inspected.
 * site is zero when no stable site is available.
 */
typedef struct vrt_error_info
{
  vrt_error code;
  const vrt_func* func;
  uintptr_t site;
} vrt_error_info;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Return the stable diagnostic text for a runtime error. */
  VRT_EXPORT const char* vrt_error_message(vrt_error error);

  /**
   * Try to invoke function, capturing any runtime error it raises.
   *
   * Returns one when function completes normally and zero when generated code
   * raises a runtime error. error receives the raised error metadata. func is
   * the active generated function when the error was raised, or null when no
   * logical frame was active. site is zero until generated code supplies a
   * stable source or instruction-site identifier.
   * The returned func pointer is borrowed and must remain alive while error is
   * inspected; compiler-emitted function descriptors have program lifetime.
   * Raising an error abandons the current generated-code invocation; execution
   * cannot resume at the point that raised it.
   * The calling native thread must already have been initialized by libvrt
   * and must be between Verona invocations, with no active logical frame.
   */
  VRT_EXPORT int vrt_try_invoke(
    vrt_invocation_function function, void* context, vrt_error_info* error);

  /** Raise a runtime error to the innermost active invocation catch point. */
  VRT_EXPORT void vrt_error_raise(vrt_error error);

#if defined(__cplusplus)
}
#endif
