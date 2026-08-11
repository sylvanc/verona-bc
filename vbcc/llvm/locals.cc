#include "codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    LLVMCodegen::LocalState::LocalState(LLVMCodegen& codegen) : codegen(codegen)
    {}

    void LLVMCodegen::LocalState::reset()
    {
      local_values.clear();
      declared_var_types.clear();
    }

    void LLVMCodegen::LocalState::declare_var(
      const Node& local_id, const LoweredType& type)
    {
      declared_var_types.insert_or_assign(
        LLVMCodegen::node_text(local_id), type);
    }

    bool LLVMCodegen::LocalState::bind_value(
      const Node& definition, const Node& local_id, const LoweredValue& value)
    {
      auto name = LLVMCodegen::node_text(local_id);
      auto declared_var = declared_var_types.find(name);

      if (
        (declared_var != declared_var_types.end()) &&
        (declared_var->second != value.type))
      {
        codegen.fail(
          definition,
          "assignment representation mismatch for variable '" + name + "'");
        return false;
      }

      local_values.insert_or_assign(name, value);
      return true;
    }

    const LoweredValue*
    LLVMCodegen::LocalState::find_value(const Node& local_id) const
    {
      auto name = LLVMCodegen::node_text(local_id);
      auto value = local_values.find(name);

      if (value == local_values.end())
        return nullptr;

      return &value->second;
    }

    std::optional<LoweredValue>
    LLVMCodegen::LocalState::extract_value(const Node& local_id)
    {
      auto name = LLVMCodegen::node_text(local_id);
      auto value = local_values.find(name);

      if (value == local_values.end())
        return {};

      auto result = value->second;
      local_values.erase(value);
      return result;
    }

    std::optional<LoweredValue>
    LLVMCodegen::LocalState::move_value(const Node& use, const Node& src)
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
    LLVMCodegen::LocalState::copy_value(const Node& use, const Node& src)
    {
      auto* source = find_value(src);

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

    bool LLVMCodegen::LocalState::drop_value(const Node& use, const Node& src)
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
