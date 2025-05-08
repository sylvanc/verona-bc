#pragma once

#include "program.h"
#include "value.h"

#include <format>
#include <verona.h>

namespace vbci
{
  struct Cown : public verona::rt::VCown<Cown>
  {
  private:
    Id type_id;
    Value content;

    Cown(Id type_id) : type_id(type_id)
    {
      inc();
    }

  public:
    static Cown* create(Id type_id)
    {
      if (type::is_cown(type_id))
        throw Value(Error::BadType);

      return new Cown(type_id);
    }

    Id cown_type_id()
    {
      return type::cown(type_id);
    }

    Id content_type_id()
    {
      return type_id;
    }

    void inc()
    {
      acquire(this);
    }

    void dec()
    {
      release(this);
    }

    Value load()
    {
      return content;
    }

    Value store(bool move, Value& v)
    {
      // Allow any cown to contain an error.
      if (!v.is_error() && !Program::get().typecheck(v.type_id(), type_id))
        throw Value(Error::BadType);

      Value next;

      if (move)
        next = std::move(v);
      else
        next = v;

      if (!next.is_sendable())
        throw Value(Error::BadStore);

      auto prev = std::move(content);
      content = std::move(next);
      return prev;
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
