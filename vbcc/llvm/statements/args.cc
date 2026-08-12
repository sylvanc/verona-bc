#include "../codegen.h"

#include <cassert>
#include <functional>
#include <optional>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    namespace
    {
      using TransferValue = std::function<std::optional<LoweredValue>(
        const Node& use, const Node& src)>;

      std::optional<LoweredValue> lower_arg(
        const Node& arg,
        const TransferValue& move_value,
        const TransferValue& copy_value)
      {
        assert(arg->type() == Arg);
        auto kind = arg / Type;
        auto src = arg / Rhs;

        if (kind == ArgMove)
          return move_value(arg, src);

        assert(kind == ArgCopy);
        return copy_value(arg, src);
      }
    }

    std::optional<std::vector<LoweredValue>> lower_args(
      const Node& args,
      const TransferValue& move_value,
      const TransferValue& copy_value)
    {
      assert(args->type() == Args);
      std::vector<LoweredValue> result;
      result.reserve(args->size());

      for (const auto& arg : *args)
      {
        auto value = lower_arg(arg, move_value, copy_value);

        if (!value)
          return {};

        result.push_back(*value);
      }

      return result;
    }
  }
}
