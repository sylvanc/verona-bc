#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_heap_array(const Node& statement)
    {
      auto region_source = locals.find_value(statement / Lhs);

      if (!region_source)
      {
        fail(statement, "heap array allocation uses an unknown region source");
        return false;
      }

      if (
        (region_source->type.runtime_type != vrt::ValueType::object) ||
        (region_source->value == nullptr))
      {
        fail(
          statement,
          "heap array allocation requires an object region source");
        return false;
      }

      return emit_array_allocation(
        statement, runtime.array_heap, {region_source->value});
    }
  }
}
