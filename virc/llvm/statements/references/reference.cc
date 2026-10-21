#include "../../codegen.h"

#include <cassert>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_reference_construction(
      const Node& statement,
      const LoweredType& reference_type,
      llvm::Function* constructor,
      std::vector<llvm::Value*> arguments,
      const std::string& name)
    {
      assert(reference_type.runtime_type == vrt::ValueType::reference);
      assert(constructor != nullptr);

      auto storage = allocate_value_storage(reference_type, name + ".storage");
      arguments.insert(arguments.begin(), value_storage_pointer(storage));
      builder.CreateCall(constructor, arguments);
      return locals.bind_value(
        statement, statement / LocalId, load_value_storage(storage, name));
    }
  }
}
