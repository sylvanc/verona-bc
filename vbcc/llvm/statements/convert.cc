#include "../codegen.h"

#include <llvm/IR/Constants.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_convert(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto source = locals.find_value(src);

      if (!source)
      {
        fail(statement, "conversion of unknown local '" + node_text(src) + "'");
        return false;
      }

      auto target = lower_type(statement / Type);
      if (!target)
        return false;

      if (target->runtime_kind == RuntimeValueKind::RawPointer)
      {
        fail(statement, "ptr is not a valid conversion target");
        return false;
      }

      const bool source_is_integer = source->type.kind == ValueKind::Bool ||
        source->type.kind == ValueKind::SignedInteger ||
        source->type.kind == ValueKind::UnsignedInteger;
      const bool source_is_float = source->type.kind == ValueKind::Float;
      const bool source_is_pointer =
        source->type.runtime_kind == RuntimeValueKind::RawPointer;
      const bool source_is_none = source->type.kind == ValueKind::None;

      if (
        !source_is_integer && !source_is_float && !source_is_pointer &&
        !source_is_none)
      {
        fail(statement, "unsupported conversion source representation");
        return false;
      }

      if (target->kind == ValueKind::None)
      {
        return locals.bind_value(
          statement, dst, LoweredValue{*target, nullptr});
      }

      auto name = strip_sigil(node_text(dst));
      llvm::Value* result = nullptr;

      if (source_is_none)
      {
        if (
          target->kind == ValueKind::Bool ||
          target->kind == ValueKind::SignedInteger ||
          target->kind == ValueKind::UnsignedInteger)
          result = llvm::ConstantInt::get(target->value_type, 0);
        else if (target->kind == ValueKind::Float)
          result = llvm::ConstantFP::get(target->value_type, 0.0);
      }
      else if (target->kind == ValueKind::Bool)
      {
        if (source_is_integer)
        {
          auto* zero = llvm::ConstantInt::get(source->type.value_type, 0);
          result = builder.CreateICmpNE(source->value, zero, name);
        }
        else if (source_is_float)
        {
          auto* zero = llvm::ConstantFP::get(source->type.value_type, 0.0);
          result = builder.CreateFCmpUNE(source->value, zero, name);
        }
        else if (source_is_pointer)
        {
          result = builder.CreateIsNotNull(source->value, name);
        }
      }
      else if (
        target->kind == ValueKind::SignedInteger ||
        target->kind == ValueKind::UnsignedInteger)
      {
        if (source_is_integer)
        {
          result = builder.CreateIntCast(
            source->value,
            target->value_type,
            source->type.kind == ValueKind::SignedInteger,
            name);
        }
        else if (source_is_float)
        {
          result = target->kind == ValueKind::SignedInteger ?
            builder.CreateFPToSI(source->value, target->value_type, name) :
            builder.CreateFPToUI(source->value, target->value_type, name);
        }
        else if (source_is_pointer)
        {
          result =
            builder.CreatePtrToInt(source->value, target->value_type, name);
        }
      }
      else if (target->kind == ValueKind::Float)
      {
        if (source_is_float)
        {
          result =
            builder.CreateFPCast(source->value, target->value_type, name);
        }
        else
        {
          auto* word_type = llvm::Type::getInt64Ty(context);
          llvm::Value* word = nullptr;

          if (source_is_integer)
          {
            word = builder.CreateIntCast(
              source->value,
              word_type,
              source->type.kind == ValueKind::SignedInteger,
              name + ".bits");
          }
          else if (source_is_pointer)
          {
            word =
              builder.CreatePtrToInt(source->value, word_type, name + ".bits");
          }

          if (word != nullptr)
            result = builder.CreateUIToFP(word, target->value_type, name);
        }
      }

      if (result == nullptr)
      {
        fail(statement, "unsupported primitive conversion");
        return false;
      }

      return locals.bind_value(statement, dst, LoweredValue{*target, result});
    }
  }
}
