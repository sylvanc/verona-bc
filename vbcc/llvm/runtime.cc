#include "codegen.h"

#include <cstdint>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::declare_runtime_functions()
    {
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* void_type = llvm::Type::getVoidTy(context);

      const auto declare = [this](
                             const char* name,
                             llvm::Type* return_type,
                             std::vector<llvm::Type*> param_types) {
        auto* function_type =
          llvm::FunctionType::get(return_type, param_types, false);
        auto* function = module.getFunction(name);

        if (function && (function->getFunctionType() != function_type))
        {
          fail(
            state.top,
            "conflicting declaration for runtime function '" +
              std::string(name) + "'");
          return static_cast<llvm::Function*>(nullptr);
        }

        if (!function)
        {
          function = llvm::Function::Create(
            function_type, llvm::GlobalValue::ExternalLinkage, name, module);
        }

        function->setCallingConv(llvm::CallingConv::C);
        return function;
      };

      runtime.thread_create = declare("vrt_thread_create", pointer_type, {});
      runtime.thread_destroy =
        declare("vrt_thread_destroy", void_type, {pointer_type});
      runtime.frame_enter =
        declare("vrt_frame_enter", pointer_type, {pointer_type, pointer_type});
      runtime.frame_leave =
        declare("vrt_frame_leave", void_type, {pointer_type, pointer_type});
      runtime.frame_prepare_tailcall = declare(
        "vrt_frame_prepare_tailcall",
        void_type,
        {pointer_type, pointer_type, pointer_type});

      return !failed;
    }

    bool LLVMCodegen::define_function_descriptors()
    {
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* id_type = llvm::Type::getInt64Ty(context);
      function_descriptor_type =
        llvm::StructType::get(context, {id_type, pointer_type});
      uint64_t descriptor_id = 1;

      for (const auto& func_state : state.functions)
      {
        auto func = func_state.func;

        if (!func)
          continue;

        auto function = functions.find(node_text(func / FunctionId));

        if (function == functions.end())
          continue;

        auto& lowered = function->second;
        auto name = node_text(func / FunctionId);
        auto* name_value = llvm::ConstantDataArray::getString(context, name);
        auto* name_global = new llvm::GlobalVariable(
          module,
          name_value->getType(),
          true,
          llvm::GlobalValue::PrivateLinkage,
          name_value,
          lowered.function->getName() + ".name");
        name_global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        llvm::Constant* indices[] = {zero, zero};
        auto* name_pointer = llvm::ConstantExpr::getInBoundsGetElementPtr(
          name_value->getType(), name_global, indices);
        auto* descriptor_value = llvm::ConstantStruct::get(
          function_descriptor_type,
          llvm::ConstantInt::get(id_type, descriptor_id++),
          name_pointer);
        lowered.descriptor = new llvm::GlobalVariable(
          module,
          function_descriptor_type,
          true,
          llvm::GlobalValue::PrivateLinkage,
          descriptor_value,
          lowered.function->getName() + ".descriptor");
      }

      return true;
    }

    bool LLVMCodegen::emit_entry_wrapper()
    {
      if (entry_wrapper == nullptr)
        return true;

      auto main = functions.find("@main");

      if (main == functions.end())
        return true;

      if (
        (main->second.descriptor == nullptr) ||
        (runtime.thread_create == nullptr) ||
        (runtime.thread_destroy == nullptr) ||
        (runtime.frame_enter == nullptr) || (runtime.frame_leave == nullptr))
      {
        fail(state.top, "LLVM entry runtime context is unavailable");
        return false;
      }

      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* null_pointer = llvm::ConstantPointerNull::get(pointer_type);
      auto* entry = llvm::BasicBlock::Create(context, "entry", entry_wrapper);
      auto* enter_frame =
        llvm::BasicBlock::Create(context, "enter_frame", entry_wrapper);
      auto* invoke_main =
        llvm::BasicBlock::Create(context, "invoke_main", entry_wrapper);
      auto* destroy_thread =
        llvm::BasicBlock::Create(context, "destroy_thread", entry_wrapper);
      auto* done = llvm::BasicBlock::Create(context, "done", entry_wrapper);

      builder.SetInsertPoint(entry);
      auto* thread = builder.CreateCall(runtime.thread_create, {}, "thread");
      builder.CreateCondBr(
        builder.CreateICmpNE(thread, null_pointer), enter_frame, done);

      builder.SetInsertPoint(enter_frame);
      auto* frame = builder.CreateCall(
        runtime.frame_enter, {thread, main->second.descriptor}, "frame");
      builder.CreateCondBr(
        builder.CreateICmpNE(frame, null_pointer), invoke_main, destroy_thread);

      builder.SetInsertPoint(invoke_main);
      auto* call = builder.CreateCall(main->second.function, {thread, frame});
      call->setCallingConv(main->second.function->getCallingConv());
      builder.CreateCall(runtime.frame_leave, {thread, frame});
      builder.CreateBr(destroy_thread);

      builder.SetInsertPoint(destroy_thread);
      builder.CreateCall(runtime.thread_destroy, {thread});
      builder.CreateBr(done);

      builder.SetInsertPoint(done);
      builder.CreateRetVoid();
      return true;
    }

    bool LLVMCodegen::emit_prepare_tailcall(
      const Node& statement, llvm::Value* function_descriptor)
    {
      if (
        (current_thread == nullptr) || (current_frame == nullptr) ||
        (runtime.frame_prepare_tailcall == nullptr) ||
        (function_descriptor == nullptr))
      {
        fail(statement, "LLVM tailcall runtime context is unavailable");
        return false;
      }

      builder.CreateCall(
        runtime.frame_prepare_tailcall,
        {current_thread, current_frame, function_descriptor});
      return true;
    }
  }
}
