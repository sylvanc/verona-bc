#pragma once

#include "value.h"

#include <unordered_map>
#include <unordered_set>

namespace vbci
{
  // TODO: more complex types?
  struct Type
  {};

  struct TypeDesc
  {
    // TODO: supertypes?

    // Precalculate an offset into the object for each field name.
    std::unordered_map<FieldId, FieldIdx> fields;

    // Precalculate a function pointer for each method name.
    std::unordered_map<FuncId, Function*> methods;

    Function* method(FuncId w)
    {
      auto find = methods.find(w);
      if (find == methods.end())
        return nullptr;

      return find->second;
    }
  };
}
