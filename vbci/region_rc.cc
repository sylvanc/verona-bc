#include "region_rc.h"

#include "array.h"
#include "object.h"

namespace vbci
{
  Object* RegionRC::object(Class& cls)
  {
    auto mem = new uint8_t[cls.size];
    auto obj = Object::create(mem, cls, Location(this));
    headers.emplace(obj);
    stack_inc();
    return obj;
  }

  Array* RegionRC::array(TypeId type_id, size_t size)
  {
    auto rep = Program::get().layout_type_id(type_id);
    auto mem = new uint8_t[Array::size_of(size, rep.second->size)];
    auto arr = Array::create(
      mem, Location(this), type_id, rep.first, size, rep.second->size);
    headers.emplace(arr);
    stack_inc();
    return arr;
  }

  void RegionRC::rfree(Header* h)
  {
    headers.erase(h);
    delete h;
  }

  void RegionRC::insert(Header* h)
  {
    headers.emplace(h);
  }

  void RegionRC::remove(Header* h)
  {
    headers.erase(h);
  }

  bool RegionRC::enable_rc()
  {
    return !finalizing;
  }

  void RegionRC::free_contents()
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
}
