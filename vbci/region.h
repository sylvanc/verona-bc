#pragma once

#include "ident.h"
#include "vbci.h"

#include <unordered_set>

namespace vbci
{
  struct Region
  {
    Region* parent;
    RC stack_rc;
    RegionType r_type;

    Region(RegionType type) : parent(nullptr), stack_rc(1), r_type(type) {}

    static Region* create(RegionType type)
    {
      return new Region(type);
    }

    void* alloc(size_t size)
    {
      return std::malloc(size);
    }

    bool is_ancestor(Region* r)
    {
      while (Region* p = r->parent)
      {
        if (p == this)
          return true;

        r = p;
      }

      return false;
    }

    bool enable_rc()
    {
      return r_type == RegionType::RegionRC;
    }

    void stack_inc()
    {
      stack_rc++;
    }

    void stack_dec()
    {
      if ((--stack_rc == 0) && (parent != nullptr))
      {
        // TODO: free the region.
      }
    }

    void clear_parent()
    {
      assert(parent != nullptr);
      parent = nullptr;

      if (stack_rc == 0)
      {
        // TODO: free the region.
        logging::Debug() << "Free region " << this << std::endl;
        // TODO: finalize and free all objects in the region.
      }
    }

    void set_parent(Region* p)
    {
      assert(parent == nullptr);
      parent = p;
    }
  };
}
