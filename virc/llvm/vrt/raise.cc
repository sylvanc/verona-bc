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

      auto storage =
        allocate_value_storage(return_type, "raised.value.storage");
      auto* type_id_value = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context), *type_id);
      builder.CreateCall(
        runtime.frame_take_raised_value,
        {type_id_value, value_storage_pointer(storage)});
      auto result = load_value_storage(storage, "raised.result");

      if (!emit_escape(function, result))
        return false;

      if (!emit_leave_frame(function))
        return false;

      if (return_type.ir_type == IRValueType::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(result.value);

      return true;
    }

  }
}
