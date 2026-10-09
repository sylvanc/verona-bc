#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_merge(const Node& statement)
    {
      auto lhs_id = statement / Lhs;
      auto rhs_id = statement / Rhs;
      auto lhs = locals.find_value(lhs_id);
      auto rhs = locals.find_value(rhs_id);

      if (!lhs)
      {
        fail(
          statement,
          "merge operation on unknown local '" + node_text(lhs_id) + "'");
        return false;
      }

      if (!rhs)
      {
        fail(
          statement,
          "merge operation on unknown local '" + node_text(rhs_id) + "'");
        return false;
      }

      const auto is_supported = [](vrt::ValueType type) {
        return (type == vrt::ValueType::object) ||
          (type == vrt::ValueType::array);
      };

      if (
        !is_supported(lhs->type.runtime_type) ||
        !is_supported(rhs->type.runtime_type))
      {
        fail(
          statement,
          "merge operation is only implemented for objects and arrays");
        return false;
      }

      if (
        (runtime.region_merge == nullptr) || (lhs->value == nullptr) ||
        (rhs->value == nullptr))
      {
        fail(statement, "merge operation runtime is unavailable");
        return false;
      }

      auto* word_type = module.getDataLayout().getIntPtrType(context);
      const auto value_type = [word_type](vrt::ValueType type) {
        return llvm::ConstantInt::get(
          word_type, static_cast<uintptr_t>(type));
      };

      builder.CreateCall(
        runtime.region_merge,
        {value_type(lhs->type.runtime_type),
         lhs->value,
         value_type(rhs->type.runtime_type),
         rhs->value});

      Node none_node = None;
      auto none_type = lower_type(none_node);
      if (!none_type)
        return false;

      return locals.bind_value(
        statement,
        statement / LocalId,
        LoweredValue{*none_type, nullptr});
    }
  }
}
