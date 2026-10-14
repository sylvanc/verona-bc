#include "../../codegen.h"

#include <llvm/IR/Constants.h>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_singleton(const Node& statement)
    {
      auto class_id = statement / ClassId;
      auto lowered_class = classes.find(node_text(class_id));

      if (lowered_class == classes.end())
      {
        fail(class_id, "singleton uses an unknown class");
        return false;
      }

      if (lowered_class->second.singleton == nullptr)
      {
        fail(statement, "singleton class has no fields");
        return false;
      }

      auto result_type = lower_class_id(class_id);

      if (!result_type)
        return false;

      return locals.bind_value(
        statement,
        statement / LocalId,
        LoweredValue{*result_type, lowered_class->second.singleton});
    }
  }
}