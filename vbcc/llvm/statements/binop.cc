#include "../codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_binop(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto lhs_id = statement / Lhs;
      auto rhs_id = statement / Rhs;
      auto lhs = locals.find_value(lhs_id);

      if (!lhs)
      {
        fail(
          statement,
          "binary operation of unknown local '" + node_text(lhs_id) + "'");
        return false;
      }

      auto rhs = locals.find_value(rhs_id);

      if (!rhs)
      {
        fail(
          statement,
          "binary operation of unknown local '" + node_text(rhs_id) + "'");
        return false;
      }

      if (lhs->type != rhs->type)
      {
        fail(statement, "binary operand representation mismatch");
        return false;
      }

      if (lhs->type.kind == ValueKind::None)
      {
        fail(statement, "binary operation on none");
        return false;
      }

      auto name = strip_sigil(node_text(dst));
      auto is_signed_integer = lhs->type.kind == ValueKind::SignedInteger;
      auto is_unsigned_integer = lhs->type.kind == ValueKind::UnsignedInteger;
      auto is_integer = is_signed_integer || is_unsigned_integer;
      auto is_bool = lhs->type.kind == ValueKind::Bool;
      llvm::Value* result = nullptr;
      auto result_type = lhs->type;

      if (!is_integer && !is_bool)
      {
        fail(statement, "unsupported binary operand representation");
        return false;
      }

      if (statement->type().in({Add, Sub, Mul, Div, Mod, Shl, Shr}))
      {
        if (!is_integer)
        {
          fail(statement, "integer binary operation requires integer operands");
          return false;
        }

        if (statement == Add)
          result = builder.CreateAdd(lhs->value, rhs->value, name);
        else if (statement == Sub)
          result = builder.CreateSub(lhs->value, rhs->value, name);
        else if (statement == Mul)
          result = builder.CreateMul(lhs->value, rhs->value, name);
        else if (statement == Div)
          result = is_signed_integer ?
            builder.CreateSDiv(lhs->value, rhs->value, name) :
            builder.CreateUDiv(lhs->value, rhs->value, name);
        else if (statement == Mod)
          result = is_signed_integer ?
            builder.CreateSRem(lhs->value, rhs->value, name) :
            builder.CreateURem(lhs->value, rhs->value, name);
        else if (statement == Shl)
          result = builder.CreateShl(lhs->value, rhs->value, name);
        else
          result = is_signed_integer ?
            builder.CreateAShr(lhs->value, rhs->value, name) :
            builder.CreateLShr(lhs->value, rhs->value, name);
      }
      else if (statement->type().in({And, Or, Xor}))
      {
        if (statement == And)
          result = builder.CreateAnd(lhs->value, rhs->value, name);
        else if (statement == Or)
          result = builder.CreateOr(lhs->value, rhs->value, name);
        else
          result = builder.CreateXor(lhs->value, rhs->value, name);
      }
      else if (statement->type().in({Eq, Ne, Lt, Le, Gt, Ge}))
      {
        Node bool_node = Bool;
        auto bool_type = lower_type(bool_node);
        assert(bool_type);
        result_type = *bool_type;

        if (statement == Eq)
          result = builder.CreateICmpEQ(lhs->value, rhs->value, name);
        else if (statement == Ne)
          result = builder.CreateICmpNE(lhs->value, rhs->value, name);
        else if (statement == Lt)
          result = is_signed_integer ?
            builder.CreateICmpSLT(lhs->value, rhs->value, name) :
            builder.CreateICmpULT(lhs->value, rhs->value, name);
        else if (statement == Le)
          result = is_signed_integer ?
            builder.CreateICmpSLE(lhs->value, rhs->value, name) :
            builder.CreateICmpULE(lhs->value, rhs->value, name);
        else if (statement == Gt)
          result = is_signed_integer ?
            builder.CreateICmpSGT(lhs->value, rhs->value, name) :
            builder.CreateICmpUGT(lhs->value, rhs->value, name);
        else
          result = is_signed_integer ?
            builder.CreateICmpSGE(lhs->value, rhs->value, name) :
            builder.CreateICmpUGE(lhs->value, rhs->value, name);
      }
      else if (statement->type().in({Min, Max}))
      {
        llvm::Value* select_lhs = nullptr;

        if (statement == Min)
        {
          select_lhs = is_signed_integer ?
            builder.CreateICmpSLT(lhs->value, rhs->value) :
            builder.CreateICmpULT(lhs->value, rhs->value);
        }
        else
        {
          select_lhs = is_signed_integer ?
            builder.CreateICmpSGT(lhs->value, rhs->value) :
            builder.CreateICmpUGT(lhs->value, rhs->value);
        }

        result = builder.CreateSelect(select_lhs, lhs->value, rhs->value, name);
      }
      else
      {
        fail(
          statement,
          "unsupported binary operator '" +
            std::string(statement->type().str()) +
            "' for the available scalar types");
        return false;
      }

      assert(result != nullptr);
      return locals.bind_value(
        statement, dst, LoweredValue{result_type, result});
    }
  }
}
