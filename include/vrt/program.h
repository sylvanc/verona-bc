#pragma once

#include "export.h"
#include "value.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  struct Class;

  /** Compiler-emitted runtime metadata for one Verona type. */
  struct TypeInfo
  {
    uintptr_t id;
    /** Runtime lifetime and tracing category. */
    ValueType value_type;
    /** Native storage size of a value with this type. */
    uintptr_t storage_size;
    /** Element type ID for arrays; zero for all other value types. */
    uintptr_t element_type_id;
  };

  /** Compiler-emitted storage and class metadata for one singleton object. */
  struct Singleton
  {
    void* storage;
    const Class* cls;
  };

  /** Static metadata required to initialize one generated Verona program. */
  struct Program
  {
    uintptr_t type_count;
    const TypeInfo* types;
    uintptr_t singleton_count;
    const Singleton* singletons;
  };
}

using vrt_type = vrt::TypeInfo;
using vrt_singleton = vrt::Singleton;
using vrt_program = vrt::Program;
#else
typedef struct vrt_class vrt_class;

/** Compiler-emitted runtime metadata for one Verona type. */
typedef struct vrt_type
{
  uintptr_t id;
  uintptr_t value_type;
  uintptr_t storage_size;
  uintptr_t element_type_id;
} vrt_type;

/** Compiler-emitted storage and class metadata for one singleton object. */
typedef struct vrt_singleton
{
  void* storage;
  const vrt_class* cls;
} vrt_singleton;

/** Static metadata required to initialize one generated Verona program. */
typedef struct vrt_program
{
  uintptr_t type_count;
  const vrt_type* types;
  uintptr_t singleton_count;
  const vrt_singleton* singletons;
} vrt_program;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Initialize process-wide runtime services. Call once per process. */
  VRT_EXPORT void vrt_runtime_init(void);

  /** Initialize compiler-emitted state for one generated program. */
  VRT_EXPORT void vrt_program_init(const vrt_program* program);

  /** Begin an invocation of an initialized generated program. */
  VRT_EXPORT void vrt_invocation_begin(void);

  /**
   * Set the process exit code.
   *
   * This function is implemented by libvrt and may be called by generated
   * Verona code.
   */
  VRT_EXPORT void set_exit_code(int32_t code);

  /** Static program metadata emitted by the LLVM backend. */
  VRT_EXPORT extern const vrt_program verona_program;

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
