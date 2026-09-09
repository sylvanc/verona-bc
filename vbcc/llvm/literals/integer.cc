#include "../codegen.h"

#include <cassert>
#include <cstdint>
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

      template<typename T>
      std::optional<int64_t> parse_signed_literal(const Node& literal)
      {
        auto value = parse_literal<T>(literal);
        if (!value)
          return {};

        return static_cast<int64_t>(*value);
      }

      template<typename T>
      std::optional<uint64_t> parse_unsigned_literal(const Node& literal)
      {
        auto value = parse_literal<T>(literal);
        if (!value)
          return {};

        return static_cast<uint64_t>(*value);
      }

      std::optional<LoweredValue> lower_integer_value(
        const Node& type,
        const Node& literal,
        const LoweredType& lowered,
        std::string& error)
      {
        llvm::Constant* constant = nullptr;

        if (lowered.kind == ValueKind::SignedInteger)
        {
          std::optional<int64_t> value;

          if (type == I8)
            value = parse_signed_literal<int8_t>(literal);
          else if (type == I16)
            value = parse_signed_literal<int16_t>(literal);
          else if (type == I32)
            value = parse_signed_literal<int32_t>(literal);
          else
            value = parse_signed_literal<int64_t>(literal);

          if (!value)
          {
            error = "invalid or out-of-range signed integer literal";
            return {};
          }

          constant = llvm::ConstantInt::getSigned(lowered.value_type, *value);
        }
        else
        {
          assert(lowered.kind == ValueKind::UnsignedInteger);
          std::optional<uint64_t> value;

          if (type == U8)
            value = parse_unsigned_literal<uint8_t>(literal);
          else if (type == U16)
            value = parse_unsigned_literal<uint16_t>(literal);
          else if (type == U32)
            value = parse_unsigned_literal<uint32_t>(literal);
          else
            value = parse_unsigned_literal<uint64_t>(literal);

          if (!value)
          {
            error = "invalid or out-of-range unsigned integer literal";
            return {};
          }

          constant = llvm::ConstantInt::get(lowered.value_type, *value, false);
        }

        return LoweredValue{lowered, constant};
      }
    }

    std::optional<LoweredValue> lower_integer_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error)
    {
      assert(literal->type().in({Bin, Oct, Hex, Int, Char}));

      if (
        type->type().in(
          {I8, I16, I32, I64, U8, U16, U32, U64, ILong, ULong, ISize, USize}))
        return lower_integer_value(type, literal, lowered, error);

      error = "integer literal requires an integer type";
      return {};
    }
  }
}
