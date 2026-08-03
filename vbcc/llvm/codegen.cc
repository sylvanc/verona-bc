#include "codegen.h"

namespace vbcc
{
  bool Bytecode::gen_llvm(const std::filesystem::path& output) const
  {
    // Destructor automatically restores the previous WFContext when this
    // function returns.
    trieste::WFContext wf_context(wfIR);
    llvm_backend::LLVMCodegen codegen(*this);
    return codegen.emit(output.empty() ? "out.ll" : output);
  }
}
