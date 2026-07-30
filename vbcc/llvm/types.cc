#include "codegen.h"

#include <llvm/IR/DerivedTypes.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    /* maps VIR types into LLVM types and function
     * signatures. */
    std::optional<LoweredType> LLVMCodegen::lower_type(const Node& type)
    {
      if (type == None)
      {
        return LoweredType{
          ValueKind::None,
          llvm::Type::getVoidTy(context),
          nullptr,
          OwnershipKind::Trivial};
      }

      if (type == I32)
      {
        auto* llvm_type = llvm::Type::getInt32Ty(context);
        return LoweredType{
          ValueKind::I32, llvm_type, llvm_type, OwnershipKind::Trivial};
      }

      if (type == Bool)
      {
        auto* llvm_type = llvm::Type::getInt1Ty(context);
        return LoweredType{
          ValueKind::Bool, llvm_type, llvm_type, OwnershipKind::Trivial};
      }

      fail(type, "unsupported type '" + std::string(type->type().str()) + "'");
      return {};
    }

    bool LLVMCodegen::same_value_representation(
      const LoweredType& lhs, const LoweredType& rhs)
    {
      return (lhs.kind == rhs.kind) && (lhs.value_type == rhs.value_type);
    }

    std::optional<std::vector<LoweredType>>
    LLVMCodegen::lower_params(const Node& params)
    {
      std::vector<LoweredType> result;
      result.reserve(params->size());

      for (const auto& param : *params)
      {
        Node type = param;

        if (param == Param)
          type = param / Type;

        auto lowered = lower_type(type);

        if (!lowered)
          return {};

        if (lowered->kind == ValueKind::None)
        {
          fail(type, "none cannot be used as an LLVM parameter type");
          return {};
        }

        result.push_back(*lowered);
      }

      return result;
    }

    llvm::FunctionType* LLVMCodegen::function_type(
      const LoweredType& return_type,
      const std::vector<LoweredType>& param_types)
    {
      std::vector<llvm::Type*> params;
      params.reserve(param_types.size());

      for (const auto& param_type : param_types)
        params.push_back(param_type.value_type);

      return llvm::FunctionType::get(return_type.value_type, params, false);
    }

    std::optional<LoweredSignature>
    LLVMCodegen::lower_signature(const Node& return_type, const Node& params)
    {
      auto lowered_return = lower_type(return_type);
      auto lowered_params = lower_params(params);

      if (!lowered_return || !lowered_params)
        return {};

      auto* type = function_type(*lowered_return, *lowered_params);
      return LoweredSignature{
        *lowered_return,
        std::move(*lowered_params),
        type,
        llvm::CallingConv::C,
        false};
    }
  }
}
