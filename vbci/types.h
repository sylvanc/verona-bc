#pragma once

#include "value.h"

#include <unordered_map>
#include <unordered_set>

namespace vbci
{
  // TODO: more complex types?
  // Unions of intersections of ClassId and mutability.
  // No generics - monomorphized?
  struct Type
  {};

  struct Class
  {
    // TODO: supertypes?

    // Precalculate an offset into the object for each field name.
    // TODO: field types?
    std::unordered_map<FieldId, FieldIdx> fields;

    // Precalculate a function pointer for each method name.
    std::unordered_map<MethodId, Function*> methods;

    Function* method(MethodId w)
    {
      auto find = methods.find(w);
      if (find == methods.end())
        return nullptr;

      return find->second;
    }
  };
}
