#pragma once

#include "ident.h"
#include "vbci.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vbci
{
  struct Typedef
  {
    std::vector<Id> type_ids;
  };

  struct Class
  {
    size_t size;
    size_t debug_info;
    Id class_id;

    // Precalculate an offset into the object for each field name.
    std::unordered_map<Id, FieldIdx> fields;
    std::vector<Id> field_types;

    // Precalculate a function pointer for each method name.
    std::unordered_map<Id, Function*> methods;

    void calc_size();
    Function* finalizer();
    Function* method(Id w);
  };
}
