#pragma once

#include "classes.h"
#include "frame.h"
#include "header.h"
#include "program.h"
#include "thread.h"

#include <format>

namespace vbci
{
  struct Object : public Header
  {
  private:
    Object(Location loc, Class& cls) : Header(loc, cls.type_id) {}

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

    uint32_t field_type_id(size_t idx)
    {
      return cls().fields.at(idx).type_id;
    }

    Class& cls()
    {
      return Program::get().cls(get_type_id());
    }

    size_t field(size_t field)
    {
      auto& c = cls();
      auto find = c.field_map.find(field);
      if (find == c.field_map.end())
        throw Value(Error::BadField);

      return find->second;
    }

    Function* finalizer()
    {
      return cls().finalizer();
    }

    void* get_pointer()
    {
      return reinterpret_cast<void*>(this + 1);
    }

    Value load(size_t idx)
    {
      auto& f = cls().fields.at(idx);
      void* addr = reinterpret_cast<uint8_t*>(this + 1) + f.offset;
      return Value::from_addr(f.value_type, addr);
    }

    Value store(bool move, size_t idx, Value& v)
    {
      auto& f = cls().fields.at(idx);

      if (!Program::get().subtype(v.type_id(), f.type_id))
        throw Value(Error::BadType);

      void* addr = reinterpret_cast<uint8_t*>(this + 1) + f.offset;
      auto prev = Value::from_addr(f.value_type, addr);

      if (!write_barrier(prev, v))
        throw Value(Error::BadStore);

      v.to_addr(f.value_type, addr, move);
      return prev;
    }

    void dec(bool reg)
    {
      if (base_dec(reg))
        return;

      // This object isn't in a cycle. It can be immediately finalized and then
      // freed.
      finalize();

      if (loc::is_immutable(location()))
        delete this;
      else
        region()->rfree(this);
    }

    void trace(std::vector<Header*>& list)
    {
      auto& f = cls().fields;

      for (size_t i = 0; i < f.size(); i++)
      {
        switch (f.at(i).value_type)
        {
          case ValueType::Object:
          case ValueType::Array:
          case ValueType::Invalid:
          {
            auto v = load(i);

            if (!v.is_header())
              return;

            auto h = v.get_header();

            // Only add mutable, heap allocated objects and arrays to the list.
            if (h->region())
              list.push_back(h);
            break;
          }

          default:
            break;
        }
      }
    }

    void immortalize()
    {
      if (location() == loc::Immortal)
        return;

      mark_immortal();
      auto& f = cls().fields;

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
      auto& c = cls();
      auto& f = c.fields;
      auto fin = c.finalizer();

      // Pass a read-only reference to the object.
      if (fin)
        Thread::run_sync(fin, Value(this, true));

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
