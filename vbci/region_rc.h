#pragma once

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

  public:
    RegionRC() : Region(), finalizing(false) {}

    Object* object(Class& cls);
    Array* array(TypeId type_id, size_t size);

    void rfree(Header* h);
    void insert(Header* h);
    void remove(Header* h);
    bool enable_rc();
    void free_contents();
  };
}
