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
    std::unordered_set<Object*> objects;
    std::unordered_set<Array*> arrays;

  protected:
    RegionRC() : Region() {}

    Object* object(Class& cls)
    {
      auto mem = std::malloc(cls.size);
      auto obj = Object::create(mem, cls, Location(this));
      objects.emplace(obj);
      stack_inc();
      return obj;
    }

    Array* array(size_t size)
    {
      auto mem = std::malloc(Array::size_of(size));
      auto arr = Array::create(mem, Location(this), size);
      arrays.emplace(arr);
      stack_inc();
      return arr;
    }

    void free(Object* obj)
    {
      objects.erase(obj);
      std::free(obj);
    }

    void free(Array* arr)
    {
      arrays.erase(arr);
      std::free(arr);
    }

    bool enable_rc()
    {
      return true;
    }

    void free_contents()
    {
      for (auto obj : objects)
        obj->finalize();

      for (auto arr : arrays)
        arr->finalize();

      for (auto obj : objects)
        std::free(obj);

      for (auto arr : arrays)
        std::free(arr);
    }
  };
}
