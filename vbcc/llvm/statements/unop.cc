#include "../codegen.h"

#include <cassert>
#include <llvm/IR/Constants.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_unop(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto source = locals.find_value(src);

      if (!source)
      {
        fail(
          statement,
          "unary operation of unknown local '" + node_text(src) + "'");
        return false;
      }

      if (source->type.kind == ValueKind::None)
      {
        fail(statement, "unary operation on none");
        return false;
      }

      auto name = strip_sigil(node_text(dst));
      auto is_signed_integer = source->type.kind == ValueKind::SignedInteger;
      auto is_unsigned_integer =
        source->type.kind == ValueKind::UnsignedInteger;
      auto is_integer = is_signed_integer || is_unsigned_integer;
      auto is_bool = source->type.kind == ValueKind::Bool;
      llvm::Value* result = nullptr;

      if (!is_integer && !is_bool)
      {
        fail(statement, "unsupported unary operand representation");
        return false;
      }

      if (statement == Neg)
      {
        if (!is_signed_integer)
        {
          fail(statement, "neg requires a signed integer operand");
          return false;
        }

        result = builder.CreateNeg(source->value, name);
      }
      else if (statement == Not)
      {
        result = builder.CreateNot(source->value, name);
      }
      else if (statement == Abs)
      {
        if (!is_signed_integer)
        {
          fail(statement, "abs requires a signed integer operand");
          return false;
        }

        auto* zero = llvm::ConstantInt::get(source->type.value_type, 0);
        auto* negative =
          builder.CreateICmpSLT(source->value, zero, name + ".negative");
        auto* negated = builder.CreateNeg(source->value, name + ".negated");
        result = builder.CreateSelect(negative, negated, source->value, name);
      }
      else
      {
        fail(
          statement,
          "unsupported unary operator '" +
            std::string(statement->type().str()) +
            "' for the available scalar types");
        return false;
      }

      assert(result != nullptr);
      return locals.bind_value(
        statement, dst, LoweredValue{source->type, result});
    }
  }
}
