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
    size_t size;
    Id class_id;

    // Precalculate an offset into the object for each field name.
    std::unordered_map<Id, FieldIdx> fields;

    // Precalculate a function pointer for each method name.
    std::unordered_map<Id, Function*> methods;

    void calc_size();
    Function* finalizer();
    Function* method(Id w);
  };
}
