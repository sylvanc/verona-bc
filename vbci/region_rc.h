#pragma once

#include "region.h"

#include <cstdlib>
#include <unordered_set>
#include <iostream>

namespace vbci
{
  struct RegionRC : public Region
  {
    friend struct Region;

  private:
    std::unordered_set<Header*> headers;
    bool finalizing;

  public:
    RegionRC() : Region(), finalizing(false)
    {
      LOG(Trace) << "Created RegionRC @" << this;
    }

    Object* object(Class& cls);
    Array* array(uint32_t type_id, size_t size);

    void rfree(Header* h);
    void insert(Header* h);
    void remove(Header* h);
    bool enable_rc();
    void free_contents();

    ~RegionRC() {
      LOG(Trace) << "Destroyed RegionRC @" << this;
    }
  };
}
