#include "../codegen.h"

#include <llvm/IR/Function.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::declare_runtime_functions()
    {
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* void_type = llvm::Type::getVoidTy(context);
      auto* i32_type = llvm::Type::getInt32Ty(context);
      auto* i64_type = llvm::Type::getInt64Ty(context);

      const auto declare = [this](
                             const char* name,
                             llvm::Type* return_type,
                             std::vector<llvm::Type*> param_types) {
        auto* function_type =
          llvm::FunctionType::get(return_type, param_types, false);
        auto* function = module.getFunction(name);

        if (function && (function->getFunctionType() != function_type))
        {
          fail(
            state.top,
            "conflicting declaration for runtime function '" +
              std::string(name) + "'");
          return static_cast<llvm::Function*>(nullptr);
        }

        if (!function)
        {
          function = llvm::Function::Create(
            function_type, llvm::GlobalValue::ExternalLinkage, name, module);
        }

        function->setCallingConv(llvm::CallingConv::C);
        return function;
      };

      runtime.frame_enter =
        declare("vrt_frame_enter", pointer_type, {pointer_type});
      runtime.frame_leave = declare("vrt_frame_leave", void_type, {});
      runtime.frame_reuse =
        declare("vrt_frame_reuse", void_type, {pointer_type});
      runtime.frame_get_raise_target =
        declare("vrt_frame_get_raise_target", i64_type, {});
      runtime.frame_set_raise_target =
        declare("vrt_frame_set_raise_target", i64_type, {i64_type});
      runtime.frame_raise_continuation =
        declare("vrt_frame_raise_continuation", pointer_type, {});
      runtime.frame_raise = declare("vrt_frame_raise", void_type, {i64_type});
      runtime.frame_take_raised_value =
        declare("vrt_frame_take_raised_value", i64_type, {});
      runtime.setjmp = declare("setjmp", i32_type, {pointer_type});

      if (runtime.frame_raise != nullptr)
        runtime.frame_raise->addFnAttr(llvm::Attribute::NoReturn);

      if (runtime.setjmp != nullptr)
        runtime.setjmp->addFnAttr(llvm::Attribute::ReturnsTwice);

      return !failed;
    }
  }
}
