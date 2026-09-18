#include "../codegen.h"

#include <functional>
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

    bool LLVMCodegen::emit_array_copy(const Node& statement)
    {
      if (runtime.array_copy == nullptr)
      {
        fail(statement, "array copy runtime is unavailable");
        return false;
      }

      auto args = statement / Args;
      if (args->size() != 5)
      {
        fail(statement, "arraycopy requires five arguments");
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

      if (
        (values->at(0).type.runtime_type != vrt::ValueType::array) ||
        (values->at(2).type.runtime_type != vrt::ValueType::array))
      {
        fail(statement, "arraycopy requires array operands");
        return false;
      }

      if (
        (values->at(1).type != *usize_type) ||
        (values->at(3).type != *usize_type) ||
        (values->at(4).type != *usize_type))
      {
        fail(statement, "arraycopy offsets and length must be usize");
        return false;
      }

      builder.CreateCall(
        runtime.array_copy,
        {values->at(0).value,
         values->at(1).value,
         values->at(2).value,
         values->at(3).value,
         values->at(4).value});

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
