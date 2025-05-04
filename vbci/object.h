#pragma once

#include "frame.h"
#include "header.h"
#include "program.h"
#include "thread.h"
#include "types.h"

#include <format>

namespace vbci
{
  struct Object : public Header
  {
  private:
    Id class_id;

    Object(Location loc, Class& cls) : Header(loc), class_id(cls.class_id) {}

  public:
    static Object* create(void* mem, Class& cls, Location loc)
    {
      return new (mem) Object(loc, cls);
    }

    Object& init(Frame* frame, Class& cls)
    {
      for (size_t i = 0; i < cls.fields.size(); i++)
        store(ArgType::Move, i, frame->arg(i));

      return *this;
    }

    Id type_id()
    {
      return type::cls(class_id);
    }

    Id field_type_id(FieldIdx idx)
    {
      return cls()->fields.at(idx).type_id;
    }

    Class* cls()
    {
      return &Program::get().cls(class_id);
    }

    FieldIdx field(Id field)
    {
      auto& program = Program::get();
      auto& cls = program.cls(class_id);
      auto find = cls.field_map.find(field);
      if (find == cls.field_map.end())
        throw Value(Error::BadField);

      return find->second;
    }

    Function* method(Id w)
    {
      return Program::get().cls(class_id).method(w);
    }

    Function* finalizer()
    {
      return Program::get().cls(class_id).finalizer();
    }

    Value load(FieldIdx idx)
    {
      auto& f = Program::get().cls(class_id).fields.at(idx);
      void* addr = reinterpret_cast<uint8_t*>(this + 1) + f.offset;
      return Value::from_addr(f.value_type, addr);
    }

    Value store(ArgType arg_type, FieldIdx idx, Value& v)
    {
      auto program = Program::get();
      auto& f = program.cls(class_id).fields.at(idx);

      if (!program.typecheck(v.type_id(), f.type_id))
        throw Value(Error::BadType);

      if (!safe_store(v))
        throw Value(Error::BadStore);

      void* addr = reinterpret_cast<uint8_t*>(this + 1) + f.offset;
      auto prev = Value::from_addr(f.value_type, addr);
      region_store(prev, v);
      v.to_addr(arg_type, addr);
      return prev;
    }

    void dec(bool reg)
    {
      if (base_dec(reg))
        return;

      // This object isn't in a cycle. It can be immediately finalized and then
      // freed.
      finalize();

      // TODO: this will get called for an immutable and will crash.
      region()->free(this);
    }

    void finalize()
    {
      auto& program = Program::get();
      auto& cls = program.cls(class_id);

      if (cls.finalizer())
        Thread::run_finalizer(this);

      for (size_t i = 0; i < cls.fields.size(); i++)
      {
        // TODO: can we be more efficient?
        load(i).field_drop();
      }
    }

    std::string to_string()
    {
      return std::format(
        "{}: {}", Program::get().di_class(cls()), static_cast<void*>(this));
    }
  };
}
