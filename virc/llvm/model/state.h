#pragma once

#include "representation.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace llvm
{
  class Constant;
  class Function;
  class GlobalVariable;
  class StructType;
}

namespace virc
{
  namespace llvm_backend
  {
    struct FunctionState
    {
      llvm::Function* function;
      LoweredSignature signature;
      llvm::GlobalVariable* descriptor = nullptr;
    };

    struct SingletonInitEntry
    {
      llvm::GlobalVariable* storage;
      llvm::GlobalVariable* cls;
    };

    struct ClassState
    {
      std::size_t type_id;
      // Aggregate memory layout and corresponding individual field
      // representations, respectively.
      llvm::StructType* fields_type;
      std::vector<LoweredType> field_types;
      llvm::GlobalVariable* cls = nullptr;
      llvm::Constant* singleton = nullptr;
    };

    struct RuntimeState
    {
      llvm::Function* error_raise = nullptr;
      llvm::Function* frame_enter = nullptr;
      llvm::Function* frame_leave = nullptr;
      llvm::Function* frame_reuse = nullptr;
      llvm::Function* frame_get_raise_target = nullptr;
      llvm::Function* frame_set_raise_target = nullptr;
      llvm::Function* frame_raise_continuation = nullptr;
      llvm::Function* frame_raise = nullptr;
      llvm::Function* frame_take_raised_value = nullptr;
      llvm::Function* thread_current_frame = nullptr;
      llvm::Function* array_new = nullptr;
      llvm::Function* array_stack = nullptr;
      llvm::Function* array_heap = nullptr;
      llvm::Function* array_region = nullptr;
      llvm::Function* array_retain = nullptr;
      llvm::Function* array_release = nullptr;
      llvm::Function* array_pin = nullptr;
      llvm::Function* array_unpin = nullptr;
      llvm::Function* array_freeze = nullptr;
      llvm::Function* array_escape = nullptr;
      llvm::Function* array_validate_tailcall = nullptr;
      llvm::Function* array_copy = nullptr;
      llvm::Function* array_fill = nullptr;
      llvm::Function* array_compare = nullptr;
      llvm::Function* object_new = nullptr;
      llvm::Function* object_stack = nullptr;
      llvm::Function* object_heap = nullptr;
      llvm::Function* object_region = nullptr;
      llvm::Function* object_class_id = nullptr;
      llvm::Function* object_retain = nullptr;
      llvm::Function* object_release = nullptr;
      llvm::Function* object_pin = nullptr;
      llvm::Function* object_unpin = nullptr;
      llvm::Function* object_freeze = nullptr;
      llvm::Function* object_escape = nullptr;
      llvm::Function* object_validate_tailcall = nullptr;
      llvm::Function* object_lookup = nullptr;
      llvm::Function* region_merge = nullptr;
      llvm::Function* reference_from_register = nullptr;
      llvm::Function* reference_from_field = nullptr;
      llvm::Function* reference_from_array = nullptr;
      llvm::Function* reference_retain = nullptr;
      llvm::Function* reference_release = nullptr;
      llvm::Function* reference_escape = nullptr;
      llvm::Function* reference_validate_tailcall = nullptr;
      llvm::Function* reference_load = nullptr;
      llvm::Function* reference_exchange = nullptr;
      llvm::Function* func_entry = nullptr;
      llvm::Function* setjmp = nullptr;
    };

    struct LibraryState
    {
      std::string path;
      std::vector<std::string> symbol_ids;

      // Generated module state used once named libraries are loaded through
      // the runtime rather than resolved directly by the native linker.
      llvm::GlobalVariable* handle_slot = nullptr;
    };

    struct SymbolState
    {
      std::size_t library_index;
      std::string linker_name;
      std::string version;
      bool vararg;
      LoweredType return_type;
      std::vector<LoweredType> param_types;

      // Process-local symbols use a direct declaration. Named libraries will
      // instead populate a per-program slot with a runtime-resolved pointer.
      llvm::Function* function = nullptr;
      llvm::GlobalVariable* function_pointer_slot = nullptr;
    };
  }
}