#include "codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredValue> LLVMCodegen::transfer_local(
      const Node& use_node,
      const Node& id_node,
      TransferKind transfer,
      const char* operation)
    {
      auto name = node_text(id_node);
      auto local_it = locals.find(name);

      if (local_it == locals.end())
      {
        fail(
          use_node,
          std::string(operation) + " of unknown local '" + name + "'");
        return {};
      }

      auto value = local_it->second;

      if (transfer == TransferKind::Copy)
      {
        // ArgCopy and Copy share this path so managed representations cannot
        // accidentally bypass their future retain operation.
        switch (value.type.ownership)
        {
          case OwnershipKind::Trivial:
            break;

          case OwnershipKind::Managed:
            fail(
              use_node,
              std::string(operation) + " cannot copy a managed value yet");
            return {};
        }
      }
      else
      {
        locals.erase(local_it);
      }

      return value;
    }

    bool LLVMCodegen::emit_copy(const Node& statement)
    {
      auto value =
        transfer_local(statement, statement / Rhs, TransferKind::Copy, "copy");

      if (!value)
        return false;

      return bind_local(statement, statement / LocalId, *value);
    }

    bool LLVMCodegen::emit_move(const Node& statement)
    {
      auto value =
        transfer_local(statement, statement / Rhs, TransferKind::Move, "move");

      if (!value)
        return false;

      return bind_local(statement, statement / LocalId, *value);
    }

    bool LLVMCodegen::emit_drop(const Node& statement)
    {
      auto value = transfer_local(
        statement, statement / LocalId, TransferKind::Move, "drop");

      if (!value)
        return false;

      // Managed representations will release here. Move has already consumed
      // the local, so trivial values need no emitted LLVM instruction.
      switch (value->type.ownership)
      {
        case OwnershipKind::Trivial:
          break;

        case OwnershipKind::Managed:
          fail(statement, "drop of a managed value is not supported");
          return false;
      }

      return true;
    }
  }
}
