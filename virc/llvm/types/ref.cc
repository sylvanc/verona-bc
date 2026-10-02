#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    lower_ref(llvm::LLVMContext& context, const Node& type)
    {
      assert(type == Ref);
      (void)type;

      auto* reference_type =
        llvm::StructType::getTypeByName(context, "vrt.reference");
      if (reference_type == nullptr)
      {
        reference_type = llvm::StructType::create(context, "vrt.reference");
        auto* word_type = llvm::Type::getIntNTy(
          context, sizeof(uintptr_t) * 8);
        auto* pointer_type = llvm::PointerType::getUnqual(context);
        reference_type->setBody(
          {word_type,
           pointer_type,
           pointer_type,
           word_type,
           word_type},
          false);
      }

      return LoweredType{
        IRValueType::Aggregate,
        vrt::ValueType::reference,
        reference_type,
        reference_type};
    }
  }
}
