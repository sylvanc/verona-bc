#include "../codegen.h"

#include <cstdint>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::define_function_descriptors()
    {
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* id_type = llvm::Type::getInt64Ty(context);
      auto* descriptor_type =
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
          descriptor_type,
          llvm::ConstantInt::get(id_type, descriptor_id++),
          name_pointer);
        lowered.descriptor = new llvm::GlobalVariable(
          module,
          descriptor_type,
          true,
          llvm::GlobalValue::PrivateLinkage,
          descriptor_value,
          lowered.function->getName() + ".descriptor");
      }

      return true;
    }
  }
}
