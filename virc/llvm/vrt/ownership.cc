#include "../codegen.h"

#include <cassert>

namespace virc
{
  namespace llvm_backend
  {
    namespace
    {
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
          if ((runtime.array_retain == nullptr) || (value.value == nullptr))
          {
            fail(use, "array retain runtime is unavailable");
            return false;
          }

          builder.CreateCall(runtime.array_retain, {value.value});
          return true;

        case vrt::ValueType::reference:
        {
          if ((runtime.reference_retain == nullptr) || (value.value == nullptr))
          {
            fail(use, "reference retain runtime is unavailable");
            return false;
          }

          auto storage =
            materialize_value_storage(use, value, "reference.retain.storage");
          if (!storage)
            return false;

          builder.CreateCall(
            runtime.reference_retain, {value_storage_pointer(*storage)});
          return true;
        }

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
          if ((runtime.array_release == nullptr) || (value.value == nullptr))
          {
            fail(use, "array release runtime is unavailable");
            return false;
          }

          builder.CreateCall(runtime.array_release, {value.value});
          return true;

        case vrt::ValueType::reference:
        {
          if (
            (runtime.reference_release == nullptr) || (value.value == nullptr))
          {
            fail(use, "reference release runtime is unavailable");
            return false;
          }

          auto storage =
            materialize_value_storage(use, value, "reference.release.storage");
          if (!storage)
            return false;

          builder.CreateCall(
            runtime.reference_release, {value_storage_pointer(*storage)});
          return true;
        }

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

    bool LLVMCodegen::emit_escape(const Node& use, const LoweredValue& value)
    {
      switch (value.type.runtime_type)
      {
        case vrt::ValueType::none:
        case vrt::ValueType::scalar:
        case vrt::ValueType::raw_pointer:
          return true;

        case vrt::ValueType::object:
          if ((runtime.object_escape == nullptr) || (value.value == nullptr))
          {
            fail(use, "object escape runtime is unavailable");
            return false;
          }

          builder.CreateCall(runtime.object_escape, {value.value});
          return true;

        case vrt::ValueType::array:
          if ((runtime.array_escape == nullptr) || (value.value == nullptr))
          {
            fail(use, "array escape runtime is unavailable");
            return false;
          }

          builder.CreateCall(runtime.array_escape, {value.value});
          return true;

        case vrt::ValueType::reference:
        {
          if ((runtime.reference_escape == nullptr) || (value.value == nullptr))
          {
            fail(use, "reference escape runtime is unavailable");
            return false;
          }

          auto storage =
            materialize_value_storage(use, value, "reference.escape.storage");
          if (!storage)
            return false;

          builder.CreateCall(
            runtime.reference_escape, {value_storage_pointer(*storage)});
          return true;
        }

        case vrt::ValueType::cown:
        case vrt::ValueType::dynamic:
        case vrt::ValueType::aggregate:
          fail(use, "escape lowering is unavailable for this value type");
          return false;
      }

      assert(false && "unhandled runtime value type");
      return false;
    }

    bool LLVMCodegen::emit_validate_tailcall(
      const Node& use, const LoweredValue& value)
    {
      llvm::Function* validation_function = nullptr;
      switch (value.type.runtime_type)
      {
        case vrt::ValueType::object:
          validation_function = runtime.object_validate_tailcall;
          break;

        case vrt::ValueType::array:
          validation_function = runtime.array_validate_tailcall;
          break;

        case vrt::ValueType::reference:
          validation_function = runtime.reference_validate_tailcall;
          break;

        default:
          return true;
      }

      if ((validation_function == nullptr) || (value.value == nullptr))
      {
        fail(use, "tailcall validation runtime is unavailable");
        return false;
      }

      if (value.type.runtime_type != vrt::ValueType::reference)
      {
        builder.CreateCall(validation_function, {value.value});
        return true;
      }

      auto storage =
        materialize_value_storage(use, value, "reference.tailcall.storage");
      if (!storage)
        return false;

      builder.CreateCall(
        validation_function, {value_storage_pointer(*storage)});
      return true;
    }
  }
}
