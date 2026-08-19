#pragma once

#include "../bytecode.h"
#include "../lang.h"
#include "blocks.h"
#include "locals.h"
#include "lowered.h"

#include <filesystem>
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
    class LLVMCodegen
    {
      friend class BasicBlockState;
      friend class LocalState;

    private:
      struct RuntimeFunctions
      {
        llvm::Function* frame_enter = nullptr;
        llvm::Function* frame_leave = nullptr;
        llvm::Function* frame_prepare_tailcall = nullptr;
      };

      /* working state */
      const Bytecode& state;
      llvm::LLVMContext context;
      llvm::Module module;
      llvm::IRBuilder<> builder;
      std::vector<LoweredLibrary> libraries;
      std::unordered_map<std::string, LoweredSymbol> symbols;
      std::unordered_map<std::string, LoweredFunction> functions;
      RuntimeFunctions runtime;
      llvm::Function* program_entry = nullptr;
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

      bool emit_retain(const Node& use, const LoweredValue& value);
      bool emit_release(const Node& use, const LoweredValue& value);

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
      bool declare_program_entry();
      bool declare_runtime_functions();
      bool define_function_descriptors();
      bool emit_program_entry();
      bool emit_prepare_tailcall(
        const Node& statement, llvm::Value* function_descriptor);
      bool emit_leave_frame(const Node& statement);

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
      bool emit_tailcall(const Node& statement, const LoweredType& return_type);
      bool
      emit_tailcall_dyn(const Node& statement, const LoweredType& return_type);
      bool emit_return(const Node& statement, const LoweredType& return_type);
      bool emit_raise(const Node& statement);
      bool emit_cond(const Node& statement);
      bool emit_jump(const Node& statement);
    };
  }
}
