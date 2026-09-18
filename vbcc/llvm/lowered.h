#pragma once

#include "../../include/vrt/value.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace llvm
{
  class Function;
  class GlobalVariable;
  class StructType;
  class Type;
  class Value;
}

namespace vbcc
{
  namespace llvm_backend
  {
    enum class IRValueType
    {
      None,
      Bool,
      SignedInteger,
      UnsignedInteger,
      Float,
      Pointer,
    };

    struct LoweredType
    {
      IRValueType ir_type;
      // Selects the runtime representation and lifetime operations associated
      // with values of this type.
      vrt::ValueType runtime_type;
      llvm::Type* llvm_type;

      // LLVM layout used when a VIR value needs addressable storage. The
      // mutable Vars use this for their function-local slots. None has no
      // storage representation and uses nullptr.
      llvm::Type* storage_type;

      bool operator==(const LoweredType&) const = default;
    };

    struct LoweredValue
    {
      LoweredType type;
      llvm::Value* value = nullptr;
    };

    struct LoweredFunction
    {
      llvm::Function* function;
      LoweredType return_type;
      std::vector<LoweredType> param_types;
      llvm::GlobalVariable* descriptor = nullptr;
    };

    struct LoweredSingleton
    {
      llvm::GlobalVariable* storage;
      llvm::GlobalVariable* cls;
    };

    struct LoweredClass
    {
      std::size_t type_id;
      llvm::StructType* payload_type;
      std::vector<LoweredType> field_types;
      llvm::GlobalVariable* cls = nullptr;
    };

    struct LoweredRuntime
    {
      llvm::Function* frame_enter = nullptr;
      llvm::Function* frame_leave = nullptr;
      llvm::Function* frame_reuse = nullptr;
      llvm::Function* frame_get_raise_target = nullptr;
      llvm::Function* frame_set_raise_target = nullptr;
      llvm::Function* frame_raise_continuation = nullptr;
      llvm::Function* frame_raise = nullptr;
      llvm::Function* frame_take_raised_value = nullptr;
      llvm::Function* object_new = nullptr;
      llvm::Function* object_heap = nullptr;
      llvm::Function* object_region = nullptr;
      llvm::Function* object_retain = nullptr;
      llvm::Function* object_release = nullptr;
      llvm::Function* object_escape = nullptr;
      llvm::Function* setjmp = nullptr;
    };

    struct LoweredLibrary
    {
      std::string path;
      std::optional<std::string> init_function_id;
      std::vector<std::string> symbol_ids;

      // Generated module state used once named libraries are loaded through
      // the runtime rather than resolved directly by the native linker.
      llvm::GlobalVariable* handle_slot = nullptr;
      llvm::Function* initializer = nullptr;
      llvm::GlobalVariable* finalizer_slot = nullptr;
    };

    struct LoweredSymbol
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
