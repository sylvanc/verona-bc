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
        store(true, i, frame->arg(i));

      return *this;
    }

    Id type_id()
    {
      return type::cls(class_id);
    }

    Id field_type_id(size_t idx)
    {
      return cls()->fields.at(idx).type_id;
    }

    Class* cls()
    {
      return &Program::get().cls(class_id);
    }

    size_t field(size_t field)
    {
      auto& program = Program::get();
      auto& cls = program.cls(class_id);
      auto find = cls.field_map.find(field);
      if (find == cls.field_map.end())
        throw Value(Error::BadField);

      return find->second;
    }

    Function* method(size_t w)
    {
      return Program::get().cls(class_id).method(w);
    }

    Function* finalizer()
    {
      return Program::get().cls(class_id).finalizer();
    }

    void* get_pointer()
    {
      return reinterpret_cast<void*>(this + 1);
    }

    Value load(size_t idx)
    {
      auto& f = Program::get().cls(class_id).fields.at(idx);
      void* addr = reinterpret_cast<uint8_t*>(this + 1) + f.offset;
      return Value::from_addr(f.value_type, addr);
    }

    Value store(bool move, size_t idx, Value& v)
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
      v.to_addr(move, addr);
      return prev;
    }

    void dec(bool reg)
    {
      if (base_dec(reg))
        return;

      // This object isn't in a cycle. It can be immediately finalized and then
      // freed.
      finalize();

      if (is_immutable())
        delete this;
      else
        region()->rfree(this);
    }

    void immortalize()
    {
      if (location() == Immortal)
        return;

      mark_immortal();
      auto& f = cls()->fields;

      for (size_t i = 0; i < f.size(); i++)
      {
        switch (f.at(i).value_type)
        {
          case ValueType::Object:
          case ValueType::Array:
          case ValueType::Invalid:
            load(i).immortalize();
            break;

          default:
            break;
        }
      }
    }

    void finalize()
    {
      auto c = cls();
      auto& f = c->fields;
      auto fin = c->finalizer();

      if (fin)
        Thread::run_sync(fin, this);

      for (size_t i = 0; i < f.size(); i++)
      {
        switch (f.at(i).value_type)
        {
          case ValueType::Object:
          case ValueType::Array:
          case ValueType::Invalid:
            load(i).field_drop();
            break;

          default:
            break;
        }
      }
    }

    std::string to_string()
    {
      return std::format(
        "{}: {}", Program::get().di_class(cls()), static_cast<void*>(this));
    }
  };
}
