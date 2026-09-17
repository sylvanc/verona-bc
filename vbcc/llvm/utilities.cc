#include "codegen.h"

#include <llvm/Support/raw_ostream.h>

namespace vbcc
{
  namespace llvm_backend
  {
    std::string LLVMCodegen::node_text(const Node& node)
    {
      return std::string(node->location().view());
    }

    std::string LLVMCodegen::strip_sigil(const std::string& name)
    {
      if (
        !name.empty() &&
        ((name.front() == '@') || (name.front() == '$') ||
         (name.front() == '^')))
        return name.substr(1);

      return name;
    }

    void LLVMCodegen::fail(const Node& node, const std::string& message)
    {
      failed = true;
      llvm::errs() << "LLVM backend: " << message << " at "
                   << std::string(node->type().str()) << "\n";
    }
  }
}
