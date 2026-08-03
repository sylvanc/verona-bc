#include "../codegen.h"

#include <cassert>
#include <cstddef>
#include <llvm/IR/BasicBlock.h>

namespace vbcc
{
  namespace llvm_backend
  {
    /* emit function body */
    bool LLVMCodegen::emit_function(const Node& func)
    {
      auto function_id = func / FunctionId;
      auto vars = func / Vars;
      auto params = func / Params;
      auto labels = func / Labels;
      auto id = node_text(function_id);
      auto function_it = functions.find(id);
      assert(function_it != functions.end());
      auto& lowered = function_it->second;

      if (labels->size() != 1)
      {
        fail(labels, "only single-block functions are supported");
        return false;
      }

      locals.reset();

      for (const auto& variable : *vars)
      {
        auto variable_type = variable / Type;
        auto type = lower_type(variable_type);

        if (!type)
          return false;

        locals.declare_var(variable / LocalId, *type);
      }

      auto argument = lowered.function->arg_begin();
      size_t param_index = 0;

      for (const auto& param : *params)
      {
        auto param_id = param / LocalId;
        assert(argument != lowered.function->arg_end());
        auto name = node_text(param_id);
        argument->setName(strip_sigil(name));
        if (!locals.bind_value(
              param,
              param_id,
              LoweredValue{lowered.param_types.at(param_index++), &*argument}))
          return false;

        ++argument;
      }

      auto label = labels->front();
      auto label_id = label / LabelId;
      auto body = label / Body;
      auto terminator = label / Return;
      auto* block = llvm::BasicBlock::Create(
        context, strip_sigil(node_text(label_id)), lowered.function);
      builder.SetInsertPoint(block);

      for (const auto& statement : *body)
      {
        if (!emit_statement(statement))
          return false;
      }

      return emit_return(terminator, lowered.return_type);
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

      if (value->type != return_type)
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
