#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_pin(const Node& statement)
    {
      auto src = statement / Rhs;
      auto source = locals.find_value(src);
      if (!source)
      {
        fail(statement, "pin operation on unknown local '" + node_text(src) + "'");
        return false;
      }

      llvm::Function* operation = nullptr;
      switch (source->type.runtime_type)
      {
        case vrt::ValueType::object:
          operation =
            statement == Pin ? runtime.object_pin : runtime.object_unpin;
          break;

        case vrt::ValueType::array:
          operation =
            statement == Pin ? runtime.array_pin : runtime.array_unpin;
          break;

        default:
          fail(
            statement,
            "pin operation is only implemented for objects and arrays");
          return false;
      }

      if ((operation == nullptr) || (source->value == nullptr))
      {
        fail(statement, "pin operation runtime is unavailable");
        return false;
      }

      builder.CreateCall(operation, {source->value});

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
