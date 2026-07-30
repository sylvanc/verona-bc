#include "codegen.h"

#include <cassert>
#include <cstdint>
#include <llvm/IR/Constants.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_const(const Node& statement)
    {
      auto type = statement / Type;
      auto lowered = lower_type(type);

      if (!lowered)
        return false;

      switch (lowered->kind)
      {
        case ValueKind::None:
          return bind_local(
            statement, statement / LocalId, LoweredValue{*lowered, nullptr});

        case ValueKind::Bool:
        {
          auto literal = statement / Rhs;

          if ((literal != True) && (literal != False))
          {
            fail(statement, "invalid bool literal");
            return false;
          }

          auto* constant =
            llvm::ConstantInt::get(lowered->value_type, literal == True);
          return bind_local(
            statement, statement / LocalId, LoweredValue{*lowered, constant});
        }

        case ValueKind::I32:
        {
          auto value = from_chars_sep_v<int32_t>(statement / Rhs);
          auto* constant =
            llvm::ConstantInt::getSigned(lowered->value_type, value);
          return bind_local(
            statement, statement / LocalId, LoweredValue{*lowered, constant});
        }
      }

      assert(false);
      return false;
    }

    bool LLVMCodegen::emit_binop(const Node& statement)
    {
      auto* lhs = lookup_local(statement, statement / Lhs, "binary operation");
      auto* rhs = lookup_local(statement, statement / Rhs, "binary operation");

      if (!lhs || !rhs)
        return false;

      if (!same_value_representation(lhs->type, rhs->type))
      {
        fail(statement, "binary operand representation mismatch");
        return false;
      }

      if (lhs->type.kind == ValueKind::None)
      {
        fail(statement, "binary operation on none");
        return false;
      }

      auto name = strip_sigil(node_text(statement / LocalId));
      auto is_i32 = lhs->type.kind == ValueKind::I32;
      auto is_bool = lhs->type.kind == ValueKind::Bool;
      llvm::Value* result = nullptr;
      auto result_type = lhs->type;

      if (!is_i32 && !is_bool)
      {
        fail(statement, "unsupported binary operand representation");
        return false;
      }

      if (statement->type().in({Add, Sub, Mul, Div, Mod, Shl, Shr}))
      {
        if (!is_i32)
        {
          fail(statement, "integer binary operation requires i32 operands");
          return false;
        }

        if (statement == Add)
          result = builder.CreateAdd(lhs->value, rhs->value, name);
        else if (statement == Sub)
          result = builder.CreateSub(lhs->value, rhs->value, name);
        else if (statement == Mul)
          result = builder.CreateMul(lhs->value, rhs->value, name);
        else if (statement == Div)
          result = builder.CreateSDiv(lhs->value, rhs->value, name);
        else if (statement == Mod)
          result = builder.CreateSRem(lhs->value, rhs->value, name);
        else if (statement == Shl)
          result = builder.CreateShl(lhs->value, rhs->value, name);
        else
          result = builder.CreateAShr(lhs->value, rhs->value, name);
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
          result = is_i32 ?
            builder.CreateICmpSLT(lhs->value, rhs->value, name) :
            builder.CreateICmpULT(lhs->value, rhs->value, name);
        else if (statement == Le)
          result = is_i32 ?
            builder.CreateICmpSLE(lhs->value, rhs->value, name) :
            builder.CreateICmpULE(lhs->value, rhs->value, name);
        else if (statement == Gt)
          result = is_i32 ?
            builder.CreateICmpSGT(lhs->value, rhs->value, name) :
            builder.CreateICmpUGT(lhs->value, rhs->value, name);
        else
          result = is_i32 ?
            builder.CreateICmpSGE(lhs->value, rhs->value, name) :
            builder.CreateICmpUGE(lhs->value, rhs->value, name);
      }
      else if (statement->type().in({Min, Max}))
      {
        llvm::Value* select_lhs = nullptr;

        if (statement == Min)
        {
          select_lhs = is_i32 ? builder.CreateICmpSLT(lhs->value, rhs->value) :
                                builder.CreateICmpULT(lhs->value, rhs->value);
        }
        else
        {
          select_lhs = is_i32 ? builder.CreateICmpSGT(lhs->value, rhs->value) :
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
      return bind_local(
        statement, statement / LocalId, LoweredValue{result_type, result});
    }

    bool LLVMCodegen::emit_unop(const Node& statement)
    {
      auto* source =
        lookup_local(statement, statement / Rhs, "unary operation");

      if (!source)
        return false;

      if (source->type.kind == ValueKind::None)
      {
        fail(statement, "unary operation on none");
        return false;
      }

      auto name = strip_sigil(node_text(statement / LocalId));
      auto is_i32 = source->type.kind == ValueKind::I32;
      auto is_bool = source->type.kind == ValueKind::Bool;
      llvm::Value* result = nullptr;

      if (!is_i32 && !is_bool)
      {
        fail(statement, "unsupported unary operand representation");
        return false;
      }

      if (statement == Neg)
      {
        if (!is_i32)
        {
          fail(statement, "neg requires an i32 operand");
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
        if (!is_i32)
        {
          fail(statement, "abs requires an i32 operand");
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
      return bind_local(
        statement, statement / LocalId, LoweredValue{source->type, result});
    }
  }
}
