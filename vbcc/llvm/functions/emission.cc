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

      if (labels->empty())
      {
        fail(labels, "function has no basic blocks");
        return false;
      }

      locals.reset();
      blocks.reset();

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

      // Create every block before emitting bodies so branches may target
      // labels that appear later in VIR source order.
      for (const auto& label : *labels)
      {
        auto label_id = label / LabelId;

        if (!blocks.declare(label_id, lowered.function))
          return false;
      }

      for (const auto& label : *labels)
      {
        auto label_id = label / LabelId;
        auto* block = blocks.get(label_id);
        builder.SetInsertPoint(block);

        for (const auto& statement : *(label / Body))
        {
          if (!emit_statement(statement))
            return false;
        }

        if (!emit_terminator(label / Return, lowered.return_type))
          return false;
      }

      return true;
    }
  }
}
