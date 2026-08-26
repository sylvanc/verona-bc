#include "../codegen.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_raise_continuation(
      const Node& function,
      llvm::BasicBlock* normal_entry,
      const LoweredType& return_type)
    {
      if (
        (normal_entry == nullptr) ||
        (runtime.frame_raise_continuation == nullptr) ||
        (runtime.frame_take_raised_value == nullptr) ||
        (runtime.setjmp == nullptr))
      {
        fail(function, "LLVM raise runtime context is unavailable");
        return false;
      }

      auto* continuation = builder.CreateCall(
        runtime.frame_raise_continuation, {}, "raise.continuation");
      auto* state =
        builder.CreateCall(runtime.setjmp, {continuation}, "raise.state");
      auto* raised =
        llvm::BasicBlock::Create(context, "raised", normal_entry->getParent());
      auto* zero = llvm::ConstantInt::get(state->getType(), 0);
      builder.CreateCondBr(
        builder.CreateICmpEQ(state, zero), normal_entry, raised);

      builder.SetInsertPoint(raised);
      auto* value =
        builder.CreateCall(runtime.frame_take_raised_value, {}, "raised.value");

      if (!emit_leave_frame(function))
        return false;

      if (return_type.kind == ValueKind::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(unpack_raised_value(return_type, value));

      return true;
    }

    std::optional<llvm::Value*> LLVMCodegen::pack_raised_value(
      const Node& statement, const LoweredValue& value)
    {
      auto* word_type = llvm::Type::getInt64Ty(context);

      if (value.type.kind == ValueKind::None)
        return llvm::ConstantInt::get(word_type, 0);

      if (value.value == nullptr)
      {
        fail(statement, "raised value has no runtime representation");
        return {};
      }

      switch (value.type.kind)
      {
        case ValueKind::Bool:
        case ValueKind::SignedInteger:
        case ValueKind::UnsignedInteger:
          return builder.CreateZExtOrTrunc(
            value.value, word_type, "raise.bits");

        case ValueKind::Float:
        {
          auto width = value.value->getType()->getPrimitiveSizeInBits();
          auto* bits_type = llvm::IntegerType::get(context, width);
          auto* bits =
            builder.CreateBitCast(value.value, bits_type, "raise.float.bits");
          return builder.CreateZExt(bits, word_type, "raise.bits");
        }

        case ValueKind::Pointer:
          return builder.CreatePtrToInt(value.value, word_type, "raise.bits");

        case ValueKind::None:
          break;
      }

      fail(statement, "unsupported raised value representation");
      return {};
    }

    llvm::Value* LLVMCodegen::unpack_raised_value(
      const LoweredType& type, llvm::Value* value)
    {
      switch (type.kind)
      {
        case ValueKind::Bool:
        case ValueKind::SignedInteger:
        case ValueKind::UnsignedInteger:
          return builder.CreateTruncOrBitCast(
            value, type.value_type, "raised.result");

        case ValueKind::Float:
        {
          auto width = type.value_type->getPrimitiveSizeInBits();
          auto* bits_type = llvm::IntegerType::get(context, width);
          auto* bits =
            builder.CreateTruncOrBitCast(value, bits_type, "raised.float.bits");
          return builder.CreateBitCast(bits, type.value_type, "raised.result");
        }

        case ValueKind::Pointer:
          return builder.CreateIntToPtr(
            value, type.value_type, "raised.result");

        case ValueKind::None:
          return nullptr;
      }

      return nullptr;
    }
  }
}
