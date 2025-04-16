#include "object.h"

namespace vbci
{
  Object* Object::create(Id class_id, Location loc, size_t fields)
  {
    auto obj =
      static_cast<Object*>(malloc(sizeof(Object) + (fields * sizeof(Value))));
    obj->class_id = class_id;
    obj->loc = loc;
    obj->rc = 0;

    for (size_t i = 0; i < fields; i++)
      obj->fields[i] = Value();

    return obj;
  }
}
