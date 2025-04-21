#pragma once

#include "frame.h"
#include "header.h"
#include "program.h"
#include "thread.h"
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

    FieldIdx field(Id field)
    {
      auto& program = Program::get();
      auto& cls = program.classes.at(class_id);
      auto find = cls.fields.find(field);
      if (find == cls.fields.end())
        throw Value(Error::BadField);

      return find->second;
    }

    Function* method(Id w)
    {
      return Program::get().classes.at(class_id).method(w);
    }

    Function* finalizer()
    {
      return Program::get().classes.at(class_id).finalizer();
    }

    Value load(FieldIdx idx)
    {
      return fields[idx];
    }

    Value store(ArgType arg_type, FieldIdx idx, Value& v)
    {
      return base_store(arg_type, fields[idx], v);
    }

    void dec(bool reg)
    {
      if (base_dec(reg))
        return;

      // This object isn't in a cycle. It can be immediately finalized and then
      // freed.
      auto& program = Program::get();
      auto& cls = program.classes.at(class_id);

      if (cls.finalizer())
        Thread::run_finalizer(this);

      for (size_t i = 0; i < cls.fields.size(); i++)
        fields[i].field_drop();

      // TODO: free the memory
    }
  };
}
