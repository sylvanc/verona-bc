#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_heap(const Node& statement)
    {
      auto region_source = locals.find_value(statement / Rhs);

      if (!region_source)
      {
        fail(statement, "heap allocation uses an unknown region source");
        return false;
      }

      if (
        (region_source->type.runtime_type != vrt::ValueType::object) ||
        (region_source->value == nullptr))
      {
        fail(statement, "heap allocation requires an object region source");
        return false;
      }

      return emit_object_allocation(
        statement, runtime.object_heap, {region_source->value});
    }
  }
}
