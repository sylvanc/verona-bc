#include "../codegen.h"

#include <llvm/IR/Constants.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_convert(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto source = locals.find_value(src);

      if (!source)
      {
        fail(statement, "conversion of unknown local '" + node_text(src) + "'");
        return false;
      }

      auto target = lower_type(statement / Type);
      if (!target)
        return false;

      if (target->runtime_type == vrt::ValueType::raw_pointer)
      {
        fail(statement, "ptr is not a valid conversion target");
        return false;
      }

      const bool source_is_integer =
        source->type.ir_type == IRValueType::Bool ||
        source->type.ir_type == IRValueType::SignedInteger ||
        source->type.ir_type == IRValueType::UnsignedInteger;
      const bool source_is_float = source->type.ir_type == IRValueType::Float;
      const bool source_is_pointer =
        source->type.runtime_type == vrt::ValueType::raw_pointer;
      const bool source_is_none = source->type.ir_type == IRValueType::None;

      if (
        !source_is_integer && !source_is_float && !source_is_pointer &&
        !source_is_none)
      {
        fail(statement, "unsupported conversion source representation");
        return false;
      }

      if (target->ir_type == IRValueType::None)
      {
        return locals.bind_value(
          statement, dst, LoweredValue{*target, nullptr});
      }

      auto name = strip_sigil(node_text(dst));
      llvm::Value* result = nullptr;

      if (source_is_none)
      {
        if (
          target->ir_type == IRValueType::Bool ||
          target->ir_type == IRValueType::SignedInteger ||
          target->ir_type == IRValueType::UnsignedInteger)
          result = llvm::ConstantInt::get(target->llvm_type, 0);
        else if (target->ir_type == IRValueType::Float)
          result = llvm::ConstantFP::get(target->llvm_type, 0.0);
      }
      else if (target->ir_type == IRValueType::Bool)
      {
        if (source_is_integer)
        {
          auto* zero = llvm::ConstantInt::get(source->type.llvm_type, 0);
          result = builder.CreateICmpNE(source->value, zero, name);
        }
        else if (source_is_float)
        {
          auto* zero = llvm::ConstantFP::get(source->type.llvm_type, 0.0);
          result = builder.CreateFCmpUNE(source->value, zero, name);
        }
        else if (source_is_pointer)
        {
          result = builder.CreateIsNotNull(source->value, name);
        }
      }
      else if (
        target->ir_type == IRValueType::SignedInteger ||
        target->ir_type == IRValueType::UnsignedInteger)
      {
        if (source_is_integer)
        {
          result = builder.CreateIntCast(
            source->value,
            target->llvm_type,
            source->type.ir_type == IRValueType::SignedInteger,
            name);
        }
        else if (source_is_float)
        {
          result = target->ir_type == IRValueType::SignedInteger ?
            builder.CreateFPToSI(source->value, target->llvm_type, name) :
            builder.CreateFPToUI(source->value, target->llvm_type, name);
        }
        else if (source_is_pointer)
        {
          result =
            builder.CreatePtrToInt(source->value, target->llvm_type, name);
        }
      }
      else if (target->ir_type == IRValueType::Float)
      {
        if (source_is_float)
        {
          result = builder.CreateFPCast(source->value, target->llvm_type, name);
        }
        else
        {
          auto* word_type = llvm::Type::getInt64Ty(context);
          llvm::Value* word = nullptr;

          if (source_is_integer)
          {
            word = builder.CreateIntCast(
              source->value,
              word_type,
              source->type.ir_type == IRValueType::SignedInteger,
              name + ".bits");
          }
          else if (source_is_pointer)
          {
            word =
              builder.CreatePtrToInt(source->value, word_type, name + ".bits");
          }

          if (word != nullptr)
            result = builder.CreateUIToFP(word, target->llvm_type, name);
        }
      }

      if (result == nullptr)
      {
        fail(statement, "unsupported primitive conversion");
        return false;
      }

      return locals.bind_value(statement, dst, LoweredValue{*target, result});
    }
  }
}
