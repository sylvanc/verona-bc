#pragma once

#include "../bytecode.h"
#include "../lang.h"

#include <cstddef>
#include <filesystem>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
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
      SignedInteger,
      UnsignedInteger,
      Float,
      Pointer,
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

    struct LoweredLibrary
    {
      std::string path;
      std::optional<std::string> init_function_id;
      std::vector<std::string> symbol_ids;

      // Generated module state used once named libraries are loaded through
      // the runtime rather than resolved directly by the native linker.
      llvm::GlobalVariable* handle_slot = nullptr;
      llvm::Function* initializer = nullptr;
      llvm::GlobalVariable* finalizer_slot = nullptr;
    };

    struct LoweredSymbol
    {
      size_t library_index;
      std::string linker_name;
      std::string version;
      bool vararg;
      LoweredType return_type;
      std::vector<LoweredType> param_types;

      // Process-local symbols use a direct declaration. Named libraries will
      // instead populate a per-program slot with a runtime-resolved pointer.
      llvm::Function* function = nullptr;
      llvm::GlobalVariable* function_pointer_slot = nullptr;
    };

    class LLVMCodegen
    {
    private:
      class BasicBlockState
      {
      private:
        LLVMCodegen& codegen;
        std::unordered_map<std::string, llvm::BasicBlock*> blocks;

      public:
        explicit BasicBlockState(LLVMCodegen& codegen);

        void reset();
        bool declare(const Node& label_id, llvm::Function* function);
        llvm::BasicBlock* get(const Node& label_id) const;
        llvm::BasicBlock* find(const Node& branch, const Node& label_id);
      };

      class LocalState
      {
      private:
        LLVMCodegen& codegen;
        // Latest SSA value seen for each live VIR register during emission.
        // This linear environment cannot merge distinct incoming values at
        // control-flow joins; that requires PHI or storage lowering.
        std::unordered_map<std::string, LoweredValue> values;
        // Function-wide declared representation of each mutable VIR Var, used
        // to reject reassignments with incompatible representations.
        std::unordered_map<std::string, LoweredType> declared_var_types;

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
      std::vector<LoweredLibrary> libraries;
      std::unordered_map<std::string, LoweredSymbol> symbols;
      std::unordered_map<std::string, LoweredFunction> functions;
      BasicBlockState blocks;
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

      void declare_libraries();
      bool emit_library_initializers();
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

      bool
      emit_terminator(const Node& terminator, const LoweredType& return_type);
      bool emit_tailcall(const Node& statement);
      bool emit_tailcall_dyn(const Node& statement);
      bool emit_return(const Node& statement, const LoweredType& return_type);
      bool emit_raise(const Node& statement);
      bool emit_cond(const Node& statement);
      bool emit_jump(const Node& statement);
    };
  }
}
