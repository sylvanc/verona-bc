#include "codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    LLVMCodegen::LLVMCodegen(const Compilation& state)
    : state(state),
      module("verona", context),
      builder(context),
      blocks(*this),
      locals(*this)
    {}
  }

  bool llvm_backend::emit(
    const Compilation& compilation, const std::filesystem::path& output)
  {
    // Destructor automatically restores the previous WFContext when this
    // function returns.
    trieste::WFContext wf_context(wfIR);
    LLVMCodegen codegen(compilation);
    return codegen.emit(output.empty() ? "out.ll" : output);
  }

  namespace llvm
  {
    bool emit(
      const Compilation& compilation, const std::filesystem::path& output)
    {
      return llvm_backend::emit(compilation, output);
    }
  }
}
