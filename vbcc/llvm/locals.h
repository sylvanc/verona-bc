#pragma once

#include "../lang.h"
#include "lowered.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace llvm
{
  class Value;
}

namespace vbcc
{
  namespace llvm_backend
  {
    class LLVMCodegen;

    class LocalState
    {
    private:
      struct VariableState
      {
        LoweredType type;
        llvm::Value* storage;
      };

      LLVMCodegen& codegen;
      // SSA values for parameters and single-assignment VIR registers.
      std::unordered_map<std::string, LoweredValue> local_values;
      // Function-local storage for mutable VIR Vars. Slotless types such as
      // None have a null storage pointer but remain declared here.
      std::unordered_map<std::string, VariableState> variables;

      // Extracts an SSA binding or loads a mutable Var without applying an
      // ownership policy.
      std::optional<LoweredValue> extract_value(const Node& local_id);

    public:
      explicit LocalState(LLVMCodegen& codegen);

      void reset();
      // Establish storage for a mutable Var.
      void declare_var(const Node& local_id, const LoweredType& type);
      // Record an SSA value or initialize/reassign mutable Var storage.
      bool bind_value(
        const Node& definition,
        const Node& local_id,
        const LoweredValue& value);
      std::optional<LoweredValue> find_value(const Node& local_id);
      std::optional<LoweredValue> move_value(const Node& use, const Node& src);
      std::optional<LoweredValue> copy_value(const Node& use, const Node& src);
      bool drop_value(const Node& use, const Node& src);
    };
  }
}
