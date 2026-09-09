#include "codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    namespace
    {
      bool
      emit_object_retain(llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

      bool emit_object_release(
        llvm::Module&, llvm::IRBuilder<>&, const LoweredValue&)
      {
        return false;
      }

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
      switch (value.type.runtime_kind)
      {
        case RuntimeValueKind::None:
        case RuntimeValueKind::Scalar:
        case RuntimeValueKind::RawPointer:
          return true;

        case RuntimeValueKind::Object:
          if (emit_object_retain(module, builder, value))
            return true;

          fail(use, "object retain lowering is not implemented");
          return false;

        case RuntimeValueKind::Array:
          if (emit_array_retain(module, builder, value))
            return true;

          fail(use, "array retain lowering is not implemented");
          return false;

        case RuntimeValueKind::Reference:
          if (emit_reference_retain(module, builder, value))
            return true;

          fail(use, "reference retain lowering is not implemented");
          return false;

        case RuntimeValueKind::Cown:
          if (emit_cown_retain(module, builder, value))
            return true;

          fail(use, "cown retain lowering is not implemented");
          return false;

        case RuntimeValueKind::Dynamic:
          if (emit_dynamic_retain(module, builder, value))
            return true;

          fail(use, "dynamic retain lowering is not implemented");
          return false;

        case RuntimeValueKind::Aggregate:
          if (emit_aggregate_retain(module, builder, value))
            return true;

          fail(use, "aggregate retain lowering is not implemented");
          return false;
      }

      assert(false && "unhandled runtime value kind");
      return false;
    }

    bool LLVMCodegen::emit_release(const Node& use, const LoweredValue& value)
    {
      switch (value.type.runtime_kind)
      {
        case RuntimeValueKind::None:
        case RuntimeValueKind::Scalar:
        case RuntimeValueKind::RawPointer:
          return true;

        case RuntimeValueKind::Object:
          if (emit_object_release(module, builder, value))
            return true;

          fail(use, "object release lowering is not implemented");
          return false;

        case RuntimeValueKind::Array:
          if (emit_array_release(module, builder, value))
            return true;

          fail(use, "array release lowering is not implemented");
          return false;

        case RuntimeValueKind::Reference:
          if (emit_reference_release(module, builder, value))
            return true;

          fail(use, "reference release lowering is not implemented");
          return false;

        case RuntimeValueKind::Cown:
          if (emit_cown_release(module, builder, value))
            return true;

          fail(use, "cown release lowering is not implemented");
          return false;

        case RuntimeValueKind::Dynamic:
          if (emit_dynamic_release(module, builder, value))
            return true;

          fail(use, "dynamic release lowering is not implemented");
          return false;

        case RuntimeValueKind::Aggregate:
          if (emit_aggregate_release(module, builder, value))
            return true;

          fail(use, "aggregate release lowering is not implemented");
          return false;
      }

      assert(false && "unhandled runtime value kind");
      return false;
    }

  }
}
