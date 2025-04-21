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
    std::unordered_set<Object*> finalizer;
    std::unordered_set<void*> no_finalizer;

    RegionRC() : Region() {}

    Object* object(Class& cls)
    {
      auto mem = std::malloc(cls.size);
      auto obj = Object::create(mem, cls, Location(this));

      if (cls.finalizer())
        finalizer.emplace(obj);
      else
        no_finalizer.emplace(obj);

      return obj;
    }

    Array* array(size_t size)
    {
      auto mem = std::malloc(Array::size_of(size));
      auto arr = Array::create(mem, Location(this), size);
      no_finalizer.emplace(arr);
      return arr;
    }

    void free(Object* obj)
    {
      if (obj->finalizer())
        finalizer.erase(obj);
      else
        no_finalizer.erase(obj);

      std::free(obj);
    }

    void free(Array* arr)
    {
      no_finalizer.erase(arr);
      std::free(arr);
    }

    bool enable_rc()
    {
      return true;
    }

    void free_contents()
    {
      for (auto obj : finalizer)
      {
        if (obj->finalizer())
          Thread::run_finalizer(obj);
      }

      for (auto obj : finalizer)
        std::free(obj);

      for (auto p : no_finalizer)
        std::free(p);
    }
  };
}
