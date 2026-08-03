#include "codegen.h"

#include <cassert>
#include <llvm/IR/BasicBlock.h>

namespace vbcc
{
  namespace llvm_backend
  {
    LLVMCodegen::BasicBlockState::BasicBlockState(LLVMCodegen& codegen)
    : codegen(codegen)
    {}

    void LLVMCodegen::BasicBlockState::reset()
    {
      blocks.clear();
    }

    bool LLVMCodegen::BasicBlockState::declare(
      const Node& label_id, llvm::Function* function)
    {
      auto name = LLVMCodegen::node_text(label_id);

      if (blocks.contains(name))
      {
        codegen.fail(label_id, "duplicate basic block '" + name + "'");
        return false;
      }

      blocks.emplace(
        name,
        llvm::BasicBlock::Create(
          codegen.context, LLVMCodegen::strip_sigil(name), function));
      return true;
    }

    llvm::BasicBlock*
    LLVMCodegen::BasicBlockState::get(const Node& label_id) const
    {
      auto block = blocks.find(LLVMCodegen::node_text(label_id));
      assert(block != blocks.end());
      return block->second;
    }

    llvm::BasicBlock*
    LLVMCodegen::BasicBlockState::find(const Node& branch, const Node& label_id)
    {
      auto name = LLVMCodegen::node_text(label_id);
      auto block = blocks.find(name);

      if (block == blocks.end())
      {
        codegen.fail(branch, "branch to unknown basic block '" + name + "'");
        return nullptr;
      }

      return block->second;
    }
  }
}
