#include "codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    LLVMCodegen::LocalState::LocalState(LLVMCodegen& codegen) : codegen(codegen)
    {}

    void LLVMCodegen::LocalState::reset()
    {
      values.clear();
      variable_types.clear();
    }

    void LLVMCodegen::LocalState::declare_var(
      const Node& id, const LoweredType& type)
    {
      variable_types.insert_or_assign(LLVMCodegen::node_text(id), type);
    }

    bool LLVMCodegen::LocalState::bind_value(
      const Node& definition, const Node& id, const LoweredValue& value)
    {
      auto name = LLVMCodegen::node_text(id);
      auto variable = variable_types.find(name);

      if (
        (variable != variable_types.end()) && (variable->second != value.type))
      {
        codegen.fail(
          definition,
          "assignment representation mismatch for variable '" + name + "'");
        return false;
      }

      values.insert_or_assign(name, value);
      return true;
    }

    const LoweredValue*
    LLVMCodegen::LocalState::find_value(const Node& id) const
    {
      auto name = LLVMCodegen::node_text(id);
      auto value = values.find(name);

      if (value == values.end())
        return nullptr;

      return &value->second;
    }

    std::optional<LoweredValue>
    LLVMCodegen::LocalState::take_value(const Node& id)
    {
      auto name = LLVMCodegen::node_text(id);
      auto value = values.find(name);

      if (value == values.end())
        return {};

      auto result = value->second;
      values.erase(value);
      return result;
    }
  }
}
