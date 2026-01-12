#include "region_rc.h"

#include "array.h"
#include "object.h"
#include "thread.h"

namespace vbci
{
  Object* RegionRC::object(Class& cls)
  {
    auto mem = new uint8_t[cls.size];
    auto loc = Thread::region_location(this);
    auto obj = Object::create(mem, cls, loc);
    headers.emplace(obj);
    stack_inc();
    return obj;
  }

  Array* RegionRC::array(uint32_t type_id, size_t size)
  {
    auto content_type_id = Program::get().unarray(type_id);
    auto rep = Program::get().layout_type_id(content_type_id);
    auto mem = new uint8_t[Array::size_of(size, rep.second->size)];
    auto loc = Thread::region_location(this);
    auto arr =
      Array::create(mem, loc, type_id, rep.first, size, rep.second->size);
    headers.emplace(arr);
    stack_inc();
    return arr;
  }

  void RegionRC::rfree(Header* h)
  {
    headers.erase(h);
    delete[] reinterpret_cast<uint8_t*>(h);
  }

  void RegionRC::insert(Header* h)
  {
    LOG(Trace) << "Inserting header @" << h << " into RegionRC @" << this;
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
    auto& program = Program::get();
    finalizing = true;

    for (auto h : headers)
    {
      if (program.is_array(h->get_type_id()))
        static_cast<Array*>(h)->finalize();
      else
        static_cast<Object*>(h)->finalize();
    }

    for (auto h : headers)
    {
      LOG(Trace) << "Deallocating header @" << h;
      delete[] reinterpret_cast<uint8_t*>(h);
    }
  }

  void RegionRC::trace(std::vector<Header*>& list) const
  {
    auto& program = Program::get();
    for (auto h : headers)
    {
      if (program.is_array(h->get_type_id()))
      {
        static_cast<Array*>(h)->trace(list);
      }
      else
      {
        static_cast<Object*>(h)->trace(list);
      }
    }
  }
}
