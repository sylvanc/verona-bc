#pragma once

#include "../lang.h"
#include "model/blocks.h"
#include "model/dispatch.h"
#include "model/locals.h"
#include "model/representation.h"
#include "model/state.h"

#include <filesystem>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace virc
{
  namespace llvm_backend
  {
    class LLVMCodegen
    {
      friend class BasicBlockState;
      friend class LocalState;

    private:
      /* working state */
      const Compilation& state;
      llvm::LLVMContext context;
      llvm::Module module;
      llvm::IRBuilder<> builder;
      std::vector<LibraryState> libraries;
      std::unordered_map<std::string, SymbolState> symbols;
      std::unordered_map<std::string, FunctionState> functions;
      std::unordered_map<std::string, ClassState> classes;
      std::vector<Node> runtime_types;
      std::vector<SingletonInitEntry> singletons;
      RuntimeState runtime;
      llvm::Function* program_entry = nullptr;
      BasicBlockState blocks;
      LocalState locals;
      bool failed = false;

    public:
      LLVMCodegen(const Compilation& state);

      bool emit(const std::filesystem::path& output);

    private:
      // Utilities.
      static std::string node_text(const Node& node);
      static std::string strip_sigil(const std::string& name);
      void fail(const Node& node, const std::string& message);

      // Module build stages, in execution order.
      bool configure_target();

      bool declare_class_types();

      bool define_class_types();

      bool declare_callables();
      void declare_libraries();
      void declare_functions();
      bool declare_program_entry();
      bool declare_runtime_functions();

      bool define_metadata();
      bool define_function_metadata();
      bool define_class_metadata();

      bool define_functions();
      bool emit_func(const Node& func);
      bool emit_program_entry();

      bool define_program_metadata();

      bool emit_initializers();
      bool emit_library_initializers();

      bool verify_and_write(const std::filesystem::path& output);

      // Lowerers.
      std::optional<LoweredType> lower_type(const Node& type);
      std::optional<LoweredType> lower_class_id(const Node& type);
      std::optional<LoweredType> lower_type_id(const Node& type);
      std::optional<LoweredType> lower_union(const Node& type);
      std::optional<std::vector<LoweredType>> lower_params(const Node& params);
      std::optional<llvm::Value*> lower_array_size(const Node& statement);

      // Resolvers.
      std::optional<std::size_t> runtime_type_id(const Node& type);
      std::optional<Node> resolve_local_type(const Node& local_id);
      std::optional<LookupPlan> resolve_lookup(const Node& statement);

      // wfStatement emitters, in token order.
      bool emit_statement(const Node& statement);
      bool emit_const(const Node& statement);
      bool emit_convert(const Node& statement);
      bool emit_singleton(const Node& statement);
      bool emit_new(const Node& statement);
      bool emit_stack(const Node& statement);
      bool emit_heap(const Node& statement);
      bool emit_region(const Node& statement);
      bool emit_new_array(const Node& statement);
      bool emit_new_array_const(const Node& statement);
      bool emit_stack_array(const Node& statement);
      bool emit_heap_array(const Node& statement);
      bool emit_region_array(const Node& statement);
      bool emit_register_ref(const Node& statement);
      bool emit_field_ref(const Node& statement);
      bool emit_array_ref(const Node& statement);
      bool emit_array_ref_const(const Node& statement);
      bool emit_load(const Node& statement);
      bool emit_store(const Node& statement);
      bool emit_copy(const Node& statement);
      bool emit_move(const Node& statement);
      bool emit_freeze(const Node& statement);
      bool emit_pin(const Node& statement);
      bool emit_merge(const Node& statement);
      bool emit_drop(const Node& statement);
      bool emit_lookup(const Node& statement);
      bool emit_call(const Node& statement);
      bool emit_call_dyn(const Node& statement);
      bool emit_ffi(const Node& statement);
      bool emit_binop(const Node& statement);
      bool emit_unop(const Node& statement);
      bool emit_add_external(const Node& statement);
      bool emit_remove_external(const Node& statement);
      bool emit_array_copy(const Node& statement);
      bool emit_array_fill(const Node& statement);
      bool emit_array_compare(const Node& statement);
      bool emit_get_raise(const Node& statement);
      bool emit_set_raise(const Node& statement);
      bool emit_const_e(const Node& statement);
      bool emit_const_pi(const Node& statement);
      bool emit_const_inf(const Node& statement);
      bool emit_const_nan(const Node& statement);

      // wfTerminator emitters, in token order.
      bool
      emit_terminator(const Node& terminator, const LoweredType& return_type);
      bool emit_tailcall(const Node& statement, const LoweredType& return_type);
      bool
      emit_tailcall_dyn(const Node& statement, const LoweredType& return_type);
      bool emit_return(const Node& statement, const LoweredType& return_type);
      bool emit_raise(const Node& statement);
      bool emit_cond(const Node& statement);
      bool emit_jump(const Node& statement);

      // Allocation helpers.
      bool emit_array_allocation(
        const Node& statement,
        llvm::Function* allocation_function,
        std::vector<llvm::Value*> prefix_arguments);
      bool emit_object_allocation(
        const Node& statement,
        llvm::Function* allocation_function,
        std::vector<llvm::Value*> prefix_arguments);

      // Ownership helpers.
      bool emit_retain(const Node& use, const LoweredValue& value);
      bool emit_release(const Node& use, const LoweredValue& value);
      bool emit_escape(const Node& use, const LoweredValue& value);
      bool
      emit_validate_tailcall(const Node& use, const LoweredValue& value);

      // Argument helpers.
      bool emit_release_args(
        const Node& args, const std::vector<LoweredValue>& values);
      std::optional<LoweredValue> transfer_arg(const Node& arg);

      // Callable helpers.
      std::optional<llvm::Value*>
      emit_callable_entry(const Node& statement, const LoweredValue& callable);

      // Frame management.
      bool
      emit_enter_frame(const Node& statement, llvm::Value* function_descriptor);
      bool
      emit_reuse_frame(const Node& statement, llvm::Value* function_descriptor);
      bool emit_leave_frame(const Node& statement);

      // Raise handling.
      bool emit_raise_continuation(
        const Node& function,
        llvm::BasicBlock* normal_entry,
        const LoweredType& return_type);
      llvm::Value* allocate_value_storage(
        const LoweredType& type, const std::string& name);
      std::optional<llvm::Value*> materialize_value_storage(
        const Node& statement,
        const LoweredValue& value,
        const std::string& name);
      llvm::Value* load_value_storage(
        const LoweredType& type,
        llvm::Value* storage,
        const std::string& name);
    };
  }
}
