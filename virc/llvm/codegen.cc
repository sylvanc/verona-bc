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

  namespace llvm
  {
    bool
    emit(const Compilation& compilation, const std::filesystem::path& output)
    {
      // Destructor automatically restores the previous WFContext when this
      // function returns.
      trieste::WFContext wf_context(wfIR);
      llvm_backend::LLVMCodegen codegen(compilation);
      return codegen.emit(output.empty() ? "out.ll" : output);
    }
  }
}
