#pragma once

#include "frame.h"
#include "header.h"
#include "types.h"

namespace vbci
{
  struct Object : public Header
  {
  private:
    Id class_id;
    Value fields[0];

    Object(Location loc, Class& cls) : Header(loc), class_id(cls.class_id)
    {
      for (size_t i = 0; i < cls.fields.size(); i++)
        fields[i] = Value();
    }

  public:
    static Object* create(void* mem, Class& cls, Location loc)
    {
      return new (mem) Object(loc, cls);
    }

    Id get_class_id()
    {
      return class_id;
    }

    Value load(FieldIdx idx)
    {
      return fields[idx];
    }

    Value store(ArgType arg_type, FieldIdx idx, Value& v)
    {
      return base_store(arg_type, fields[idx], v);
    }
  };
}
