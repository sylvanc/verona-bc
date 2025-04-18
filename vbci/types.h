#pragma once

#include "ident.h"
#include "vbci.h"

#include <unordered_map>
#include <unordered_set>

namespace vbci
{
  struct Type
  {};

  struct Class
  {
    // Precalculate an offset into the object for each field name.
    std::unordered_map<Id, FieldIdx> fields;

    // Precalculate a function pointer for each method name.
    std::unordered_map<Id, Function*> methods;

    Function* method(Id w)
    {
      auto find = methods.find(w);
      if (find == methods.end())
        return nullptr;

      return find->second;
    }
  };
}
