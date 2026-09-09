#include "locals.h"

#include "codegen.h"

#include <llvm/IR/Value.h>

namespace vbcc
{
  namespace llvm_backend
  {
    LocalState::LocalState(LLVMCodegen& codegen) : codegen(codegen) {}

    void LocalState::reset()
    {
      local_values.clear();
      variables.clear();
    }

    void LocalState::declare_var(const Node& local_id, const LoweredType& type)
    {
      auto name = LLVMCodegen::node_text(local_id);
      llvm::Value* storage = nullptr;

      if (type.storage_type != nullptr)
      {
        storage = codegen.builder.CreateAlloca(
          type.storage_type, nullptr, LLVMCodegen::strip_sigil(name) + ".slot");
      }

      variables.insert_or_assign(name, VariableState{type, storage});
    }

    bool LocalState::bind_value(
      const Node& definition, const Node& local_id, const LoweredValue& value)
    {
      auto name = LLVMCodegen::node_text(local_id);
      auto variable = variables.find(name);

      if (
        (variable != variables.end()) && (variable->second.type != value.type))
      {
        codegen.fail(
          definition,
          "assignment representation mismatch for variable '" + name + "'");
        return false;
      }

      if (variable != variables.end())
      {
        auto& state = variable->second;

        if (state.storage == nullptr)
          return true;

        if (
          (value.value == nullptr) ||
          (value.value->getType() != state.type.storage_type))
        {
          codegen.fail(
            definition,
            "assignment storage mismatch for variable '" + name + "'");
          return false;
        }

        codegen.builder.CreateStore(value.value, state.storage);
        return true;
      }

      local_values.insert_or_assign(name, value);
      return true;
    }

    std::optional<LoweredValue> LocalState::find_value(const Node& local_id)
    {
      auto name = LLVMCodegen::node_text(local_id);
      auto variable = variables.find(name);

      if (variable != variables.end())
      {
        auto& state = variable->second;

        if (state.storage == nullptr)
          return LoweredValue{state.type, nullptr};

        auto* value = codegen.builder.CreateLoad(
          state.type.storage_type,
          state.storage,
          LLVMCodegen::strip_sigil(name) + ".load");
        return LoweredValue{state.type, value};
      }

      auto value = local_values.find(name);

      if (value == local_values.end())
        return {};

      return value->second;
    }

    std::optional<LoweredValue> LocalState::extract_value(const Node& local_id)
    {
      auto name = LLVMCodegen::node_text(local_id);
      auto variable = variables.find(name);

      if (variable != variables.end())
        return find_value(local_id);

      auto value = local_values.find(name);

      if (value == local_values.end())
        return {};

      auto result = value->second;
      local_values.erase(value);
      return result;
    }

    std::optional<LoweredValue>
    LocalState::move_value(const Node& use, const Node& src)
    {
      auto value = extract_value(src);

      if (!value)
      {
        codegen.fail(
          use, "move of unknown local '" + LLVMCodegen::node_text(src) + "'");
        return {};
      }

      // Moving transfers the source's existing ownership obligation.
      return value;
    }

    std::optional<LoweredValue>
    LocalState::copy_value(const Node& use, const Node& src)
    {
      auto source = find_value(src);

      if (!source)
      {
        codegen.fail(
          use, "copy of unknown local '" + LLVMCodegen::node_text(src) + "'");
        return {};
      }

      auto value = *source;

      // Copying creates another ownership obligation when the runtime
      // representation requires one.
      if (!codegen.emit_retain(use, value))
        return {};

      return value;
    }

    bool LocalState::drop_value(const Node& use, const Node& src)
    {
      auto value = extract_value(src);

      if (!value)
      {
        codegen.fail(
          use, "drop of unknown local '" + LLVMCodegen::node_text(src) + "'");
        return false;
      }

      // Dropping ends the binding's ownership obligation when the runtime
      // representation requires one.
      return codegen.emit_release(use, *value);
    }
  }
}
