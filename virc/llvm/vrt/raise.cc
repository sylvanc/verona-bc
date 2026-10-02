#include "../codegen.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>

namespace virc
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
      auto type_id = runtime_type_id(function / Type);
      if (!type_id)
        return false;

      auto* storage =
        allocate_value_storage(return_type, "raised.value.storage");
      auto* type_id_value = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context), *type_id);
      builder.CreateCall(
        runtime.frame_take_raised_value, {type_id_value, storage});
      auto* result =
        load_value_storage(return_type, storage, "raised.result");

      if (!emit_escape(function, LoweredValue{return_type, result}))
        return false;

      if (!emit_leave_frame(function))
        return false;

      if (return_type.ir_type == IRValueType::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(result);

      return true;
    }

    llvm::Value* LLVMCodegen::allocate_value_storage(
      const LoweredType& type, const std::string& name)
    {
      if (type.storage_type == nullptr)
        return llvm::ConstantPointerNull::get(
          llvm::PointerType::getUnqual(context));

      auto* block = builder.GetInsertBlock();
      assert(block != nullptr);
      auto& entry = block->getParent()->getEntryBlock();
      llvm::IRBuilder<> entry_builder(context);
      entry_builder.SetInsertPoint(&entry, entry.begin());
      return entry_builder.CreateAlloca(type.storage_type, nullptr, name);
    }

    std::optional<llvm::Value*> LLVMCodegen::materialize_value_storage(
      const Node& statement,
      const LoweredValue& value,
      const std::string& name)
    {
      if (value.type.ir_type == IRValueType::None)
        return allocate_value_storage(value.type, name);

      if ((value.value == nullptr) || (value.type.storage_type == nullptr))
      {
        fail(statement, "value has no storage representation");
        return {};
      }

      auto* storage = allocate_value_storage(value.type, name);
      builder.CreateStore(value.value, storage);
      return storage;
    }

    llvm::Value* LLVMCodegen::load_value_storage(
      const LoweredType& type,
      llvm::Value* storage,
      const std::string& name)
    {
      if (type.storage_type == nullptr)
        return nullptr;

      return builder.CreateLoad(type.storage_type, storage, name);
    }
  }
}
