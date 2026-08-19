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

      runtime.frame_enter =
        declare("vrt_frame_enter", pointer_type, {pointer_type});
      runtime.frame_leave = declare("vrt_frame_leave", void_type, {});
      runtime.frame_prepare_tailcall =
        declare("vrt_frame_prepare_tailcall", void_type, {pointer_type});

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

    bool LLVMCodegen::emit_prepare_tailcall(
      const Node& statement, llvm::Value* function_descriptor)
    {
      if (
        (runtime.frame_prepare_tailcall == nullptr) ||
        (function_descriptor == nullptr))
      {
        fail(statement, "LLVM tailcall runtime context is unavailable");
        return false;
      }

      builder.CreateCall(runtime.frame_prepare_tailcall, {function_descriptor});
      return true;
    }

    bool LLVMCodegen::emit_leave_frame(const Node& statement)
    {
      if (runtime.frame_leave == nullptr)
      {
        fail(statement, "LLVM frame runtime is unavailable");
        return false;
      }

      builder.CreateCall(runtime.frame_leave);
      return true;
    }

  }
}
