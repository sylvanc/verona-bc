#pragma once

#include "header.h"

namespace vbci
{
  struct Object : public Header
  {
    Id class_id;
    Value fields[0];

    static Object* create(Id class_id, Location loc, size_t fields)
    {
      auto obj =
        static_cast<Object*>(malloc(sizeof(Object) + (fields * sizeof(Value))));
      obj->class_id = class_id;
      obj->loc = loc;
      obj->rc = 1;
  
      for (size_t i = 0; i < fields; i++)
        obj->fields[i] = Value();
  
      return obj;
    }
  
    Value store(FieldIdx idx, Value& v)
    {
      // TODO: type_check
      return base_store(fields[idx], v);
    }
  };
}
