#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<std::vector<LoweredType>>
    LLVMCodegen::lower_params(const Node& params)
    {
      std::vector<LoweredType> result;
      result.reserve(params->size());

      for (const auto& param : *params)
      {
        Node type = param;

        if (param == Param)
          type = param / Type;

        auto lowered = lower_type(type);

        if (!lowered)
          return {};

        if (lowered->kind == ValueKind::None)
        {
          fail(type, "none cannot be used as an LLVM parameter type");
          return {};
        }

        result.push_back(*lowered);
      }

      return result;
    }
  }
}
