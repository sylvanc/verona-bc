#pragma once

#include "function.h"
#include "value.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** Runtime representation of a field in a generated class payload. */
  struct Field
  {
    uintptr_t offset;
    uintptr_t size;
    uintptr_t type_id;
    ValueType value_type;
  };

  /** Runtime representation used for a method dispatch-table entry. */
  struct Method
  {
    uintptr_t id;
    const Func* func;
  };

  /** Static metadata for one generated class. */
  struct Class
  {
    uintptr_t id;
    const char* name;
    uintptr_t payload_size;
    uintptr_t payload_alignment;
    uintptr_t field_count;
    const Field* fields;
    uintptr_t method_count;
    const Method* methods;
    /**
     * Compiler-emitted immortal payload for an empty class.
     *
     * Non-empty classes set this to null.
     */
    void* singleton;
  };
}

using vrt_field = vrt::Field;
using vrt_method = vrt::Method;
using vrt_class = vrt::Class;
#else
/** Runtime representation of a field in a generated class payload. */
typedef struct vrt_field
{
  uintptr_t offset;
  uintptr_t size;
  uintptr_t type_id;
  uintptr_t value_type;
} vrt_field;

/** Runtime representation used for a method dispatch-table entry. */
typedef struct vrt_method
{
  uintptr_t id;
  const vrt_func* func;
} vrt_method;

/** Static metadata for one generated class. */
typedef struct vrt_class
{
  uintptr_t id;
  const char* name;
  uintptr_t payload_size;
  uintptr_t payload_alignment;
  uintptr_t field_count;
  const vrt_field* fields;
  uintptr_t method_count;
  const vrt_method* methods;
  void* singleton;
} vrt_class;
#endif
