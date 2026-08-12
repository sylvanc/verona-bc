#include "codegen.h"

#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <memory>
#include <optional>
#include <system_error>

namespace vbcc
{
  namespace llvm_backend
  {
    /* top-level LLVM module emission entry point */
    bool LLVMCodegen::emit(const std::filesystem::path& output)
    {
      if (!configure_target())
        return false;

      if (!predeclare_nominal_types())
        return false;

      if (!define_type_layouts())
        return false;

      if (!declare_callables())
        return false;

      if (!define_globals_and_metadata())
        return false;

      if (!define_functions())
        return false;

      if (!emit_initializers())
        return false;

      return verify_and_write(output);
    }

    bool LLVMCodegen::configure_target()
    {
      // Configure the module for LLVM's native/default target.
      // Keep the module triple and DataLayout consistent by deriving them
      // from the same TargetMachine.
      // For AOT compilation later, support user-specified target triple
      // through CLI override. For now, just use the default target triple.
      llvm::Triple target_triple(llvm::sys::getDefaultTargetTriple());

      if (llvm::InitializeNativeTarget())
      {
        llvm::errs() << "LLVM backend: could not initialize target for '"
                     << target_triple.str() << "'\n";
        return false;
      }

      std::string error;
      const auto* target =
        llvm::TargetRegistry::lookupTarget(target_triple, error);

      if (target == nullptr)
      {
        llvm::errs() << "LLVM backend: could not resolve target '"
                     << target_triple.str() << "': " << error << "\n";
        return false;
      }

      llvm::TargetOptions options;
      auto target_machine =
        std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
          target_triple, "generic", "", options, std::nullopt));

      if (!target_machine)
      {
        llvm::errs() << "LLVM backend: could not create target machine for '"
                     << target_triple.str() << "'\n";
        return false;
      }

      module.setTargetTriple(target_triple);
      module.setDataLayout(target_machine->createDataLayout());
      return true;
    }

    bool LLVMCodegen::predeclare_nominal_types()
    {
      // The currently supported scalar VIR types are LLVM context-owned
      // primitives created on demand by lower_type(), so they need no module
      // declarations. Future class lowering will create opaque named
      // StructTypes here, allowing recursive references before layouts exist.
      return true;
    }

    bool LLVMCodegen::define_type_layouts()
    {
      // Type aliases do not produce LLVM entities. Future class lowering will
      // resolve field storage types and set the bodies of the opaque
      // StructTypes declared by predeclare_nominal_types().
      return true;
    }

    bool LLVMCodegen::declare_callables()
    {
      // Declare every native symbol and Verona function before emitting any
      // body so forward calls and recursion do not depend on VIR source order.
      declare_libraries();
      declare_functions();
      if (!declare_runtime_functions())
        return false;

      return !failed;
    }

    bool LLVMCodegen::define_globals_and_metadata()
    {
      // Class and primitive descriptors will also belong here because their
      // method tables refer to the functions declared by declare_callables().
      //
      // Memo globals also belong here so MemoSlot can refer to them while
      // define_functions() emits function bodies. emit_initializers() will
      // later generate the code that fills those globals.
      return define_function_descriptors();
    }

    bool LLVMCodegen::define_functions()
    {
      for (const auto& func_state : state.functions)
      {
        if (func_state.func && !emit_function(func_state.func))
          return false;
      }

      return emit_entry_wrapper() && !failed;
    }

    bool LLVMCodegen::emit_initializers()
    {
      // This phase will eventually call the initialization functions in
      // MemoInit order and store their results in the globals declared by
      // define_globals_and_metadata().
      for (const auto& child : *state.top)
      {
        if ((child->type() == MemoInit) && (child->size() != 0))
        {
          fail(child, "MemoInit lowering is not supported");
          return false;
        }
      }
      // emit_memo_initializers() will eventually emit the MemoInit functions in
      // order, but for now we just emit the FFI library initializers.
      return emit_library_initializers();
    }

    bool LLVMCodegen::verify_and_write(const std::filesystem::path& output)
    {
      if (llvm::verifyModule(module, &llvm::errs()))
      {
        llvm::errs() << "LLVM backend: module verification failed\n";
        return false;
      }

      std::error_code error;
      llvm::raw_fd_ostream stream(
        output.string(), error, llvm::sys::fs::OF_Text);

      if (error)
      {
        llvm::errs() << "LLVM backend: could not open '" << output.string()
                     << "': " << error.message() << "\n";
        return false;
      }
      /* textual LLVM IR output */
      module.print(stream, nullptr);
      stream.flush();

      if (stream.has_error())
      {
        llvm::errs() << "LLVM backend: could not write '" << output.string()
                     << "'\n";
        return false;
      }

      return true;
    }
  }
}
