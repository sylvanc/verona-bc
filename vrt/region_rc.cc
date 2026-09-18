#include "region_rc.h"

#include "failure.h"
#include "header.h"
#include "object.h"

#include <new>
#include <vector>

namespace vrt
{
  Object* RegionRC::object(const Class* cls)
  {
    if (destroying || finalizing)
      fail(Failure::invalid_region_state);

    auto* allocation = new (std::nothrow) std::byte[Object::size_of(cls)];
    if (allocation == nullptr)
      fail(Failure::out_of_memory);

    auto* result = Object::create(allocation, cls, this);
    insert(result);
    stack_inc();
    return result;
  }

  void RegionRC::insert(Header* header)
  {
    if ((header == nullptr) || (header->region() != this))
      fail(Failure::invalid_region_state);

    headers.emplace(header);
  }

  bool RegionRC::remove(Header* header)
  {
    return headers.erase(header) != 0;
  }

  bool RegionRC::contains(Header* header) const
  {
    return headers.find(header) != headers.end();
  }

  size_t RegionRC::header_count() const
  {
    return headers.size();
  }

  bool RegionRC::is_finalizing() const
  {
    return finalizing;
  }

  bool RegionRC::begin_finalizing()
  {
    if (finalizing)
      return false;

    finalizing = true;
    return true;
  }

  void RegionRC::finalize_contents()
  {
    if (!finalizing)
      fail(Failure::invalid_region_state);

    for (auto* header : headers)
      header->finalize();
  }

  void RegionRC::release_dead_objects()
  {
    if (!finalizing)
      fail(Failure::invalid_region_state);

    std::vector<Header*> dead;
    dead.reserve(headers.size());
    for (auto* header : headers)
      dead.push_back(header);

    headers.clear();
    for (auto* header : dead)
      header->destroy_storage();

    delete this;
  }
}
