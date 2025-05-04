#pragma once

#include "ident.h"
#include "value.h"

#include <ffi.h>
#include <unordered_map>
#include <unordered_set>
#include <vbci.h>
#include <vector>

namespace vbci
{
  struct Typedef
  {
    std::vector<Id> type_ids;
  };

  struct Field
  {
    size_t offset;
    size_t size;
    Id type_id;
    ValueType value_type;
  };

  struct Class
  {
    size_t size;
    size_t debug_info;
    Id class_id;

    // Precalculate an offset into the object for each field name.
    std::unordered_map<Id, FieldIdx> field_map;
    std::vector<Field> fields;

    // Precalculate a function pointer for each method name.
    std::unordered_map<Id, Function*> methods;

    bool calc_size(std::vector<ffi_type*>& ffi_types);
    Function* finalizer();
    Function* method(Id w);
  };
}
