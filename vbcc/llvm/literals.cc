#include "codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredValue> lower_integer_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error);

    std::optional<LoweredValue> lower_non_integer_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error);

    std::optional<LoweredValue> lower_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error)
    {
      assert(literal->type().in(
        {None, True, False, Bin, Oct, Hex, Int, Char, Float, HexFloat}));

      if (literal->type().in({Bin, Oct, Hex, Int, Char}))
        return lower_integer_literal(type, literal, lowered, error);

      return lower_non_integer_literal(type, literal, lowered, error);
    }
  }
}
