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

    Object(Frame* frame, Location loc, Class& cls)
    : Header(loc), class_id(cls.class_id)
    {
      for (size_t i = 0; i < cls.fields.size(); i++)
        store(ArgType::Move, i, frame->arg(i));
    }

  public:
    static Object* create(Frame* frame, void* mem, Class& cls, Location loc)
    {
      return new (mem) Object(frame, loc, cls);
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
