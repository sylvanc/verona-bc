#include "../codegen.h"

#include <cassert>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

namespace virc
{
  namespace llvm_backend
  {
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
      const Node& statement, const LoweredValue& value, const std::string& name)
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
      const LoweredType& type, llvm::Value* storage, const std::string& name)
    {
      if (type.storage_type == nullptr)
        return nullptr;

      return builder.CreateLoad(type.storage_type, storage, name);
    }
  }
}
