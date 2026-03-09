#pragma once

#include "region.h"
#include "program.h"

#include <cstdlib>
#include <iostream>
#include <unordered_set>

namespace vbci
{
  struct RegionRC : public Region
  {
    friend struct Region;

  private:
    std::unordered_set<Header*> headers;
    bool finalizing;

  public:
    RegionRC(RegionType type, size_t frame_depth = 0)
    : Region(type, frame_depth), finalizing(false)
    {
      LOG(Trace) << "Created RegionRC @" << this;
    }

    Object* object(Class& cls) override;
    Array* array(uint32_t type_id, size_t size) override;

    void rfree(Header* h) override;
    void insert(Header* h) override;
    void remove(Header* h) override;
    bool is_finalizing() override;
    void finalize_contents() override;
    void release_dead_objects() override;

    std::vector<Header*> get_headers() const override
    {
      return std::vector<Header*>(headers.begin(), headers.end());
    }

    ~RegionRC()
    {
      LOG(Trace) << "Destroyed RegionRC @" << this;
    }

    void trace_fn(auto&& fn) const;
  };
}
