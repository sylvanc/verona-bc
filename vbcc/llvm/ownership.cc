#include "codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    namespace
    {
      bool
      emit_array_retain(llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool
      emit_array_release(llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_reference_retain(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_reference_release(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool
      emit_cown_retain(llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool
      emit_cown_release(llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_dynamic_retain(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_dynamic_release(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_aggregate_retain(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_aggregate_release(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }
    }

    bool LLVMCodegen::emit_retain(const Node& use, const LoweredValue& value)
    {
      switch (value.type.runtime_type)
      {
        case vrt::ValueType::none:
        case vrt::ValueType::scalar:
        case vrt::ValueType::raw_pointer:
          return true;

        case vrt::ValueType::object:
          if ((runtime.object_retain == nullptr) || (value.value == nullptr))
          {
            fail(use, "object retain runtime is unavailable");
            return false;
          }

          builder.CreateCall(runtime.object_retain, {value.value});
          return true;

        case vrt::ValueType::array:
          if (emit_array_retain(module, builder, value))
            return true;

          fail(use, "array retain lowering is not implemented");
          return false;

        case vrt::ValueType::reference:
          if (emit_reference_retain(module, builder, value))
            return true;

          fail(use, "reference retain lowering is not implemented");
          return false;

        case vrt::ValueType::cown:
          if (emit_cown_retain(module, builder, value))
            return true;

          fail(use, "cown retain lowering is not implemented");
          return false;

        case vrt::ValueType::dynamic:
          if (emit_dynamic_retain(module, builder, value))
            return true;

          fail(use, "dynamic retain lowering is not implemented");
          return false;

        case vrt::ValueType::aggregate:
          if (emit_aggregate_retain(module, builder, value))
            return true;

          fail(use, "aggregate retain lowering is not implemented");
          return false;
      }

      assert(false && "unhandled runtime value type");
      return false;
    }

    bool LLVMCodegen::emit_release(const Node& use, const LoweredValue& value)
    {
      switch (value.type.runtime_type)
      {
        case vrt::ValueType::none:
        case vrt::ValueType::scalar:
        case vrt::ValueType::raw_pointer:
          return true;

        case vrt::ValueType::object:
          if ((runtime.object_release == nullptr) || (value.value == nullptr))
          {
            fail(use, "object release runtime is unavailable");
            return false;
          }

          builder.CreateCall(runtime.object_release, {value.value});
          return true;

        case vrt::ValueType::array:
          if (emit_array_release(module, builder, value))
            return true;

          fail(use, "array release lowering is not implemented");
          return false;

        case vrt::ValueType::reference:
          if (emit_reference_release(module, builder, value))
            return true;

          fail(use, "reference release lowering is not implemented");
          return false;

        case vrt::ValueType::cown:
          if (emit_cown_release(module, builder, value))
            return true;

          fail(use, "cown release lowering is not implemented");
          return false;

        case vrt::ValueType::dynamic:
          if (emit_dynamic_release(module, builder, value))
            return true;

          fail(use, "dynamic release lowering is not implemented");
          return false;

        case vrt::ValueType::aggregate:
          if (emit_aggregate_release(module, builder, value))
            return true;

          fail(use, "aggregate release lowering is not implemented");
          return false;
      }

      assert(false && "unhandled runtime value type");
      return false;
    }

  }
}
