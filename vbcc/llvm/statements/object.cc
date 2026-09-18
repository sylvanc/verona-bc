#include "../codegen.h"

#include <cassert>
#include <functional>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <optional>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    using TransferValue = std::function<std::optional<LoweredValue>(
      const Node& use, const Node& src)>;

    std::optional<std::vector<LoweredValue>> lower_args(
      const Node& args,
      const TransferValue& move_value,
      const TransferValue& copy_value);

    bool LLVMCodegen::emit_object_allocation(
      const Node& statement,
      llvm::Function* allocation_function,
      std::vector<llvm::Value*> prefix_arguments)
    {
      if (allocation_function == nullptr)
      {
        fail(statement, "object allocation runtime is unavailable");
        return false;
      }

      auto class_id = statement / ClassId;
      auto lowered_class = classes.find(node_text(class_id));

      if (lowered_class == classes.end())
      {
        fail(class_id, "object allocation uses an unknown class");
        return false;
      }

      auto& cls = lowered_class->second;
      auto args = statement / Args;

      if (args->size() != cls.field_types.size())
      {
        fail(statement, "wrong number of object initializer arguments");
        return false;
      }

      auto lowered_args = lower_args(
        args,
        [this](const Node& use, const Node& src) {
          return locals.move_value(use, src);
        },
        [this](const Node& use, const Node& src) {
          return locals.copy_value(use, src);
        });

      if (!lowered_args)
        return false;

      llvm::Value* arguments_pointer =
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context));

      if (!lowered_args->empty())
      {
        auto* current_block = builder.GetInsertBlock();
        assert(current_block != nullptr);
        auto* function = current_block->getParent();
        assert(function != nullptr);
        auto& entry = function->getEntryBlock();
        llvm::IRBuilder<> entry_builder(context);
        entry_builder.SetInsertPoint(&entry, entry.begin());
        auto* arguments = entry_builder.CreateAlloca(
          cls.payload_type,
          nullptr,
          strip_sigil(node_text(statement / LocalId)) + ".object.args");

        for (std::size_t index = 0; index < lowered_args->size(); ++index)
        {
          const auto& value = lowered_args->at(index);

          if (value.type != cls.field_types.at(index))
          {
            fail(args->at(index), "object argument representation mismatch");
            return false;
          }

          if (value.type.runtime_type == vrt::ValueType::none)
          {
            assert(value.value == nullptr);
            continue;
          }

          assert(value.value != nullptr);
          auto* address =
            builder.CreateStructGEP(cls.payload_type, arguments, index);
          builder.CreateStore(value.value, address);
        }

        arguments_pointer = arguments;
      }

      prefix_arguments.push_back(cls.cls);
      prefix_arguments.push_back(
        llvm::ConstantInt::get(
          module.getDataLayout().getIntPtrType(context), args->size()));
      prefix_arguments.push_back(arguments_pointer);
      auto* result = builder.CreateCall(
        allocation_function,
        prefix_arguments,
        strip_sigil(node_text(statement / LocalId)));
      auto result_type = lower_class_id_type(class_id);

      if (!result_type)
        return false;

      return locals.bind_value(
        statement, statement / LocalId, LoweredValue{*result_type, result});
    }
  }
}
