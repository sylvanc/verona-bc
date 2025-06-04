#pragma once

#include "array.h"
#include "object.h"
#include "region.h"

#include <cstdlib>
#include <unordered_set>

namespace vbci
{
  struct RegionRC : public Region
  {
    friend struct Region;

  private:
    std::unordered_set<Header*> headers;
    bool finalizing;

  protected:
    RegionRC() : Region(), finalizing(false) {}

    Object* object(Class& cls)
    {
      auto mem = new uint8_t[cls.size];
      auto obj = Object::create(mem, cls, Location(this));
      headers.emplace(obj);
      stack_inc();
      return obj;
    }

    Array* array(TypeId type_id, size_t size)
    {
      auto rep = Program::get().layout_type_id(type_id);
      auto mem = new uint8_t[Array::size_of(size, rep.second->size)];
      auto arr = Array::create(
        mem, Location(this), type_id, rep.first, size, rep.second->size);
      headers.emplace(arr);
      stack_inc();
      return arr;
    }

    void rfree(Header* h)
    {
      headers.erase(h);
      delete h;
    }

    void remove(Header* h)
    {
      headers.erase(h);
    }

    bool enable_rc()
    {
      return !finalizing;
    }

    void free_contents()
    {
      for (auto h : headers)
      {
        if (h->is_array())
          static_cast<Array*>(h)->finalize();
        else
          static_cast<Object*>(h)->finalize();
      }

      for (auto h : headers)
        delete h;
    }
  };
}
