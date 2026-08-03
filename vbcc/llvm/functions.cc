#include "codegen.h"

#include <cassert>
#include <cstddef>
#include <llvm/IR/BasicBlock.h>

namespace vbcc
{
  namespace llvm_backend
  {
    /* emit function bodies */
    bool LLVMCodegen::emit_function(const Node& func)
    {
      auto id = node_text(func / FunctionId);
      auto function_it = functions.find(id);
      assert(function_it != functions.end());
      auto& lowered = function_it->second;
      auto labels = func / Labels;

      if (labels->size() != 1)
      {
        fail(labels, "only single-block functions are supported");
        return false;
      }

      locals.reset();

      for (const auto& variable : *(func / Vars))
      {
        auto type = lower_type(variable / Type);

        if (!type)
          return false;

        locals.declare_var(variable / LocalId, *type);
      }

      auto argument = lowered.function->arg_begin();
      size_t param_index = 0;

      for (const auto& param : *(func / Params))
      {
        assert(argument != lowered.function->arg_end());
        auto name = node_text(param / LocalId);
        argument->setName(strip_sigil(name));
        if (!locals.bind_value(
              param,
              param / LocalId,
              LoweredValue{
                lowered.signature.param_types.at(param_index++), &*argument}))
          return false;
        ++argument;
      }

      auto label = labels->front();
      auto* block = llvm::BasicBlock::Create(
        context, strip_sigil(node_text(label / LabelId)), lowered.function);
      builder.SetInsertPoint(block);

      for (const auto& statement : *(label / Body))
      {
        if (!emit_statement(statement))
          return false;
      }

      return emit_return(label / Return, lowered.signature.return_type);
    }

    bool LLVMCodegen::emit_statement(const Node& statement)
    {
      if ((statement == Source) || (statement == Offset))
        return true;

      if (statement == Const)
        return emit_const(statement);

      if (statement->type().in({Add, Sub, Mul, Div, Mod, Pow,     And,
                                Or,  Xor, Shl, Shr, Eq,  Ne,      Lt,
                                Le,  Gt,  Ge,  Min, Max, LogBase, Atan2}))
        return emit_binop(statement);

      if (statement->type().in({Neg,  Not,     Abs,   Ceil,  Floor, Exp,
                                Log,  Sqrt,    Cbrt,  IsInf, IsNaN, Sin,
                                Cos,  Tan,     Asin,  Acos,  Atan,  Sinh,
                                Cosh, Tanh,    Asinh, Acosh, Atanh, Bits,
                                Len,  MakePtr, Read}))
        return emit_unop(statement);

      if (statement == Copy)
        return emit_copy(statement);

      if (statement == Move)
        return emit_move(statement);

      if (statement == FFI)
        return emit_ffi(statement);

      if (statement == Drop)
        return emit_drop(statement);

      fail(
        statement,
        "unsupported statement '" + std::string(statement->type().str()) + "'");
      return false;
    }

    bool LLVMCodegen::emit_return(
      const Node& statement, const LoweredType& return_type)
    {
      if (statement != Return)
      {
        fail(statement, "only return terminators are supported");
        return false;
      }

      auto value_id = statement / LocalId;
      auto* value = locals.find_value(value_id);

      if (!value)
      {
        fail(
          statement, "return of unknown local '" + node_text(value_id) + "'");
        return false;
      }

      if (!same_value_representation(value->type, return_type))
      {
        fail(statement, "return representation mismatch");
        return false;
      }

      if (return_type.kind == ValueKind::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(value->value);

      return true;
    }
  }
}
