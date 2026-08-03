#include "../codegen.h"

#include <cassert>
#include <llvm/IR/Constants.h>

namespace vbcc
{
  namespace llvm_backend
  {
    namespace
    {
      template<typename T>
      std::optional<T> parse_literal(const Node& literal)
      {
        T value = 0;
        auto result = from_chars_sep(literal, value);

        if (result.ec != std::errc())
          return {};

        return value;
      }

      std::optional<LoweredValue> lower_none_literal(
        const Node& type,
        const Node& literal,
        const LoweredType& lowered,
        std::string& error)
      {
        assert(literal == None);

        if (type != None)
        {
          error = "none literal requires the none type";
          return {};
        }

        return LoweredValue{lowered, nullptr};
      }

      std::optional<LoweredValue> lower_bool_literal(
        const Node& type,
        const Node& literal,
        const LoweredType& lowered,
        std::string& error)
      {
        assert(literal->type().in({True, False}));

        if (type != Bool)
        {
          error = "bool literal requires the bool type";
          return {};
        }

        auto* constant =
          llvm::ConstantInt::get(lowered.value_type, literal == True);
        return LoweredValue{lowered, constant};
      }

      std::optional<LoweredValue> lower_float_literal(
        const Node& type,
        const Node& literal,
        const LoweredType& lowered,
        std::string& error)
      {
        assert(literal->type().in({Float, HexFloat}));

        if (!type->type().in({F32, F64}))
        {
          error = "floating-point literal requires a floating-point type";
          return {};
        }

        llvm::Constant* constant = nullptr;

        if (type == F32)
        {
          auto value = parse_literal<float>(literal);
          if (value)
            constant = llvm::ConstantFP::get(lowered.value_type, *value);
        }
        else
        {
          auto value = parse_literal<double>(literal);
          if (value)
            constant = llvm::ConstantFP::get(lowered.value_type, *value);
        }

        if (!constant)
        {
          error = "invalid or out-of-range floating-point literal";
          return {};
        }

        return LoweredValue{lowered, constant};
      }
    }

    std::optional<LoweredValue> lower_non_integer_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error)
    {
      assert(literal->type().in({None, True, False, Float, HexFloat}));

      if (literal == None)
        return lower_none_literal(type, literal, lowered, error);

      if (literal->type().in({True, False}))
        return lower_bool_literal(type, literal, lowered, error);

      return lower_float_literal(type, literal, lowered, error);
    }
  }
}
