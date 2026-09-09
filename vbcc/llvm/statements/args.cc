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
        assert(arg->type() == Arg);
        auto kind = arg / Type;
        auto src = arg / Rhs;
        assert(kind->type().in({ArgMove, ArgCopy}));

        auto value =
          kind == ArgMove ? move_value(arg, src) : copy_value(arg, src);

        if (!value)
          return {};

        result.push_back(*value);
      }

      return result;
    }
  }
}
