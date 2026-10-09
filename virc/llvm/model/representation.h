// Lowered value representations shared by LLVM code generation.
#pragma once

#include "../../../include/vrt/value.h"

#include <optional>
#include <utility>
#include <vector>

namespace llvm
{
  class Type;
  class Value;
}

namespace virc
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
      Function,
      Aggregate,
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

    struct LoweredSignature
    {
      LoweredType return_type;
      std::vector<LoweredType> param_types;

      bool operator==(const LoweredSignature&) const = default;
    };

    struct LoweredStorage
    {
      LoweredType type;
      llvm::Value* address = nullptr;
    };

    struct LoweredValue
    {
      LoweredType type;
      llvm::Value* value = nullptr;
      std::optional<LoweredSignature> signature;

      LoweredValue(
        LoweredType type,
        llvm::Value* value,
        std::optional<LoweredSignature> signature = {})
      : type(type), value(value), signature(std::move(signature))
      {}
    };
  }
}