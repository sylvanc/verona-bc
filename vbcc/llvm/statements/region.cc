#include "../codegen.h"

#include <cassert>
#include <llvm/IR/Constants.h>
#include <vrt/region.h>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_region(const Node& statement)
    {
      auto region = statement / Region;
      assert(region->type().in({RegionRC, RegionArena}));
      auto region_type =
        region == RegionRC ? vrt::RegionType::rc : vrt::RegionType::arena;
      auto* value = llvm::ConstantInt::get(
        llvm::Type::getInt8Ty(context), static_cast<unsigned>(region_type));
      return emit_object_allocation(statement, runtime.object_region, {value});
    }
  }
}
