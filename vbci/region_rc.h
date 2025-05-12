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
      auto mem = new uint8_t[cls.size];
      auto obj = Object::create(mem, cls, Location(this));
      objects.emplace(obj);
      stack_inc();
      return obj;
    }

    Array* array(Id type_id, size_t size)
    {
      auto rep = Program::get().layout_type_id(type_id);
      auto mem = new uint8_t[Array::size_of(size, rep.second->size)];
      auto arr = Array::create(
        mem, Location(this), type_id, rep.first, size, rep.second->size);
      arrays.emplace(arr);
      stack_inc();
      return arr;
    }

    void rfree(Object* obj)
    {
      objects.erase(obj);
      delete obj;
    }

    void rfree(Array* arr)
    {
      arrays.erase(arr);
      delete arr;
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
        delete obj;

      for (auto arr : arrays)
        delete arr;
    }
  };
}
