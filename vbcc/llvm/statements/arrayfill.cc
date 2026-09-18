#include "../codegen.h"

#include <cassert>
#include <functional>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
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

    bool LLVMCodegen::emit_array_fill(const Node& statement)
    {
      if (runtime.array_fill == nullptr)
      {
        fail(statement, "array fill runtime is unavailable");
        return false;
      }

      auto args = statement / Args;
      if (args->size() != 4)
      {
        fail(statement, "arrayfill requires four arguments");
        return false;
      }

      auto values = lower_args(
        args,
        [this](const Node& use, const Node& src) {
          return locals.move_value(use, src);
        },
        [this](const Node& use, const Node& src) {
          return locals.copy_value(use, src);
        });

      if (!values)
        return false;

      Node usize_node = USize;
      auto usize_type = lower_type(usize_node);
      if (!usize_type)
        return false;

      if (values->at(0).type.runtime_type != vrt::ValueType::array)
      {
        fail(statement, "arrayfill requires an array operand");
        return false;
      }

      if (
        (values->at(1).type != *usize_type) ||
        (values->at(2).type != *usize_type))
      {
        fail(statement, "arrayfill offset and length must be usize");
        return false;
      }

      const auto& fill = values->at(3);
      auto* fill_storage_type = fill.type.storage_type;
      if (fill_storage_type == nullptr)
        fill_storage_type = llvm::Type::getInt8Ty(context);

      auto* current_block = builder.GetInsertBlock();
      assert(current_block != nullptr);
      auto* function = current_block->getParent();
      assert(function != nullptr);
      auto& entry = function->getEntryBlock();
      llvm::IRBuilder<> entry_builder(context);
      entry_builder.SetInsertPoint(&entry, entry.begin());
      auto* fill_storage = entry_builder.CreateAlloca(
        fill_storage_type,
        nullptr,
        strip_sigil(node_text(statement / LocalId)) + ".array.fill");

      if (fill.type.runtime_type != vrt::ValueType::none)
      {
        if (
          (fill.value == nullptr) ||
          (fill.value->getType() != fill_storage_type))
        {
          fail(args->at(3), "arrayfill value has no storage representation");
          return false;
        }

        builder.CreateStore(fill.value, fill_storage);
      }

      builder.CreateCall(
        runtime.array_fill,
        {values->at(0).value,
         values->at(1).value,
         values->at(2).value,
         fill_storage});

      if (!emit_release_args(args, *values))
        return false;

      Node none_node = None;
      auto none_type = lower_type(none_node);
      if (!none_type)
        return false;

      return locals.bind_value(
        statement, statement / LocalId, LoweredValue{*none_type, nullptr});
    }
  }
}
