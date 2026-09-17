#pragma once

#include <cstdint>
#include <vrt/program.h>

namespace vrt
{
  struct TypeLayout
  {
    ValueType value_type;
    uintptr_t storage_size;
  };

  /** Resolve a compiler-emitted type ID to its native storage layout. */
  TypeLayout layout_type_id(uintptr_t type_id);
}
