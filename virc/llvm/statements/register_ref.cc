#include "../codegen.h"

#include <llvm/IR/Constants.h>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_register_ref(const Node& statement)
    {
      auto src = statement / Rhs;
      auto variable = locals.find_variable_address(src);
      if (!variable || (variable->storage == nullptr))
      {
        fail(statement, "registerref requires addressable Var storage");
        return false;
      }

      auto content_type = resolve_local_type(src);
      auto reference_type = resolve_local_type(statement / LocalId);
      if (!content_type || !reference_type)
        return false;

      auto lowered_reference = lower_type(*reference_type);
      auto content_type_id = runtime_type_id(*content_type);
      if (!lowered_reference || !content_type_id)
        return false;

      if (
        (lowered_reference->runtime_type != vrt::ValueType::reference) ||
        (runtime.thread_current_frame == nullptr) ||
        (runtime.reference_from_register == nullptr))
      {
        fail(statement, "register reference runtime is unavailable");
        return false;
      }

      auto* storage =
        allocate_value_storage(*lowered_reference, "registerref.storage");
      auto* frame =
        builder.CreateCall(runtime.thread_current_frame, {}, "current.frame");
      auto* type_id = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context), *content_type_id);
      builder.CreateCall(
        runtime.reference_from_register,
        {storage, frame, variable->storage, type_id});
      auto* result =
        load_value_storage(*lowered_reference, storage, "registerref");
      return locals.bind_value(
        statement,
        statement / LocalId,
        LoweredValue{*lowered_reference, result});
    }
  }
}
