#pragma once

#include "../bytecode.h"
#include "../lang.h"

#include <filesystem>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    enum class ValueKind
    {
      None,
      Bool,
      I32,
    };

    enum class OwnershipKind
    {
      Trivial,
      Managed,
    };

    struct LoweredType
    {
      ValueKind kind;
      llvm::Type* value_type;

      // LLVM layout used when a VIR value needs addressable storage. The
      // current single-block Vars stay as SSA values; None has no storage
      // representation and uses nullptr.
      llvm::Type* storage_type;

      // Selects the lifetime policy used by Copy, ArgCopy, and Drop. Managed
      // is reserved until the runtime retain/release ABI is lowered.
      OwnershipKind ownership;

      bool operator==(const LoweredType&) const = default;
    };

    struct LoweredValue
    {
      LoweredType type;
      llvm::Value* value = nullptr;
    };

    struct LoweredFunction
    {
      llvm::Function* function;
      LoweredType return_type;
      std::vector<LoweredType> param_types;
    };

    class LLVMCodegen
    {
    private:
      class LocalState
      {
      private:
        LLVMCodegen& codegen;
        // Current SSA value for every live VIR register, including mutable
        // Vars.
        std::unordered_map<std::string, LoweredValue> values;
        // Declared representations of mutable VIR Vars. These guard
        // reassignment; current values still live in values while functions
        // are one block.
        std::unordered_map<std::string, LoweredType> variable_types;

      public:
        explicit LocalState(LLVMCodegen& codegen);

        void reset();
        void declare_var(const Node& id, const LoweredType& type);
        bool bind_value(
          const Node& definition, const Node& id, const LoweredValue& value);
        const LoweredValue* find_value(const Node& id) const;
        std::optional<LoweredValue> take_value(const Node& id);
      };

      /* working state */
      const Bytecode& state;
      llvm::LLVMContext context;
      llvm::Module module;
      llvm::IRBuilder<> builder;
      std::unordered_map<std::string, LoweredFunction> symbols;
      std::unordered_map<std::string, LoweredFunction> functions;
      LocalState locals;
      bool failed = false;

    public:
      LLVMCodegen(const Bytecode& state);

      bool emit(const std::filesystem::path& output);

    private:
      static std::string node_text(const Node& node);
      static std::string strip_sigil(const std::string& name);

      void fail(const Node& node, const std::string& message);

      std::optional<LoweredType> lower_type(const Node& type);
      std::optional<std::vector<LoweredType>> lower_params(const Node& params);

      // Module construction phases consume the normalized VIR tree and lookup
      // indexes already built in Bytecode by assignids.
      bool configure_target();
      bool predeclare_nominal_types();
      bool define_type_layouts();
      bool declare_callables();
      bool define_globals_and_metadata();
      bool define_functions();
      bool emit_initializers();
      bool verify_and_write(const std::filesystem::path& output);

      void declare_symbols();
      void declare_functions();

      bool emit_function(const Node& func);
      bool emit_statement(const Node& statement);
      bool emit_const(const Node& statement);
      bool emit_binop(const Node& statement);
      bool emit_unop(const Node& statement);
      bool emit_copy(const Node& statement);
      bool emit_move(const Node& statement);
      bool emit_drop(const Node& statement);
      bool emit_ffi(const Node& statement);
      bool emit_return(const Node& statement, const LoweredType& return_type);
    };
  }
}
