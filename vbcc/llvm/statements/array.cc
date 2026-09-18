#include "../codegen.h"

#include <llvm/IR/Constants.h>
#include <string>

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredValue> lower_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error);

    std::optional<llvm::Value*>
    LLVMCodegen::lower_array_size(const Node& statement)
    {
      Node usize_type = USize;
      auto lowered_usize = lower_type(usize_type);
      if (!lowered_usize)
        return {};

      if (statement->type().in(
            {NewArrayConst, HeapArrayConst, RegionArrayConst}))
      {
        std::string error;
        auto size = lower_literal(
          usize_type, statement / Rhs, *lowered_usize, error);
        if (!size)
        {
          fail(statement, error);
          return {};
        }

        return size->value;
      }

      auto size = locals.find_value(statement / Rhs);
      if (
        !size || (size->type != *lowered_usize) || (size->value == nullptr))
      {
        fail(statement, "array allocation requires a usize length");
        return {};
      }

      return size->value;
    }

    bool LLVMCodegen::emit_array_allocation(
      const Node& statement,
      llvm::Function* allocation_function,
      std::vector<llvm::Value*> prefix_arguments)
    {
      if (allocation_function == nullptr)
      {
        fail(statement, "array allocation runtime is unavailable");
        return false;
      }

      auto size = lower_array_size(statement);
      if (!size)
        return false;

      auto array_type = statement / Type;

      if ((array_type != Array) || (array_type->size() != 1))
      {
        fail(array_type, "array allocation requires an array type");
        return false;
      }

      auto lowered_array = lower_type(array_type);

      if (!lowered_array)
        return false;

      auto array_type_id = runtime_type_id(array_type);

      if (!array_type_id)
        return false;

      auto* word_type = module.getDataLayout().getIntPtrType(context);
      prefix_arguments.push_back(
        llvm::ConstantInt::get(word_type, *array_type_id));
      prefix_arguments.push_back(*size);
      auto* result = builder.CreateCall(
        allocation_function,
        prefix_arguments,
        strip_sigil(node_text(statement / LocalId)));

      return locals.bind_value(
        statement,
        statement / LocalId,
        LoweredValue{*lowered_array, result});
    }
  }
}
