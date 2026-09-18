#pragma once

#include "region.h"

#include <unordered_set>

namespace vrt
{
  /** Reference-counted region that tracks each contained allocation. */
  struct RegionRC : public Region
  {
    friend struct Region;

  private:
    std::unordered_set<Header*> headers;
    bool finalizing = false;

  protected:
    RegionRC(RegionType type, uintptr_t frame_depth = 0)
    : Region(type, frame_depth)
    {}

  public:
    Object* object(const Class* cls) override;
    Array* array(uintptr_t type_id, uintptr_t size) override;
    void insert(Header* header) override;
    bool remove(Header* header) override;
    bool contains(Header* header) const override;
    size_t header_count() const override;
    bool is_finalizing() const override;
    bool begin_finalizing() override;
    void finalize_contents() override;
    void release_dead_objects() override;
  };
}
