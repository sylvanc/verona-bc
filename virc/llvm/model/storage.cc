#include "../codegen.h"

#include <cassert>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

namespace virc
{
  namespace llvm_backend
  {
    LoweredStorage LLVMCodegen::allocate_value_storage(
      const LoweredType& type, const std::string& name)
    {
      if (type.storage_type == nullptr)
        return LoweredStorage{type};

      auto* block = builder.GetInsertBlock();
      assert(block != nullptr);
      auto& entry = block->getParent()->getEntryBlock();
      llvm::IRBuilder<> entry_builder(context);
      entry_builder.SetInsertPoint(&entry, entry.begin());
      return LoweredStorage{
        type, entry_builder.CreateAlloca(type.storage_type, nullptr, name)};
    }

    std::optional<LoweredStorage> LLVMCodegen::materialize_value_storage(
      const Node& statement, const LoweredValue& value, const std::string& name)
    {
      if (value.type.storage_type == nullptr)
        return allocate_value_storage(value.type, name);

      if (value.value == nullptr)
      {
        fail(statement, "value has no storage representation");
        return {};
      }

      auto storage = allocate_value_storage(value.type, name);
      assert(storage.address != nullptr);
      builder.CreateStore(value.value, storage.address);
      return storage;
    }

    LoweredValue LLVMCodegen::load_value_storage(
      const LoweredStorage& storage, const std::string& name)
    {
      if (storage.type.storage_type == nullptr)
      {
        assert(storage.address == nullptr);
        return LoweredValue{storage.type, nullptr};
      }

      assert(storage.address != nullptr);
      return LoweredValue{
        storage.type,
        builder.CreateLoad(storage.type.storage_type, storage.address, name)};
    }

    llvm::Value*
    LLVMCodegen::value_storage_pointer(const LoweredStorage& storage)
    {
      if (storage.address != nullptr)
        return storage.address;

      assert(storage.type.storage_type == nullptr);
      return llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(context));
    }
  }
}
