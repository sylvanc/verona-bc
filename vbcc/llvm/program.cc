#include "codegen.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::declare_program_entry()
    {
      auto main = functions.find("@main");

      if (main == functions.end())
        return true;

      if (module.getFunction("verona_program_entry"))
      {
        fail(state.top, "duplicate LLVM function name 'verona_program_entry'");
        return false;
      }

      auto* entry_type =
        llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
      program_entry = llvm::Function::Create(
        entry_type,
        llvm::GlobalValue::ExternalLinkage,
        "verona_program_entry",
        module);
      program_entry->setCallingConv(llvm::CallingConv::C);
      return true;
    }

    bool LLVMCodegen::emit_program_entry()
    {
      if (program_entry == nullptr)
        return true;

      auto main = functions.find("@main");

      if (main == functions.end())
      {
        fail(state.top, "LLVM main function is unavailable");
        return false;
      }

      if (
        (main->second.function == nullptr) ||
        (main->second.descriptor == nullptr))
      {
        fail(state.top, "LLVM entry function is unavailable");
        return false;
      }

      auto* entry = llvm::BasicBlock::Create(context, "entry", program_entry);

      builder.SetInsertPoint(entry);
      if (!emit_enter_frame(state.top, main->second.descriptor))
        return false;

      auto* call = builder.CreateCall(main->second.function);
      call->setCallingConv(main->second.function->getCallingConv());
      builder.CreateRetVoid();
      return true;
    }
  }
}
