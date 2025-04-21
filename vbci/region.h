#pragma once

#include "ident.h"
#include "types.h"
#include "vbci.h"

namespace vbci
{
  struct Region
  {
    friend struct Header;

  private:
    Region* parent;
    RC stack_rc;

  protected:
    Region() : parent(nullptr), stack_rc(1) {}
    virtual ~Region() = default;

  public:
    static Region* create(RegionType type);

    virtual Object* object(Class& cls) = 0;
    virtual Array* array(size_t size) = 0;
    virtual void free(Object* obj) = 0;
    virtual void free(Array* arr) = 0;

  private:
    virtual bool enable_rc() = 0;
    virtual void free_contents() = 0;

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

    void stack_inc()
    {
      stack_rc++;
    }

    bool stack_dec()
    {
      // Returns false if the region has beeen freed.
      if ((--stack_rc == 0) && (parent == nullptr))
      {
        free_region();
        return false;
      }

      return true;
    }

    void clear_parent()
    {
      // The stack RC must be greater than 0, because this only happens when
      // a region entry point is returned as the previous value during a store,
      // which means it's stack RC was just incremented.
      assert(parent != nullptr);
      assert(stack_rc > 0);
      parent = nullptr;
    }

    void set_parent(Region* p)
    {
      // This originate from a region entry point being stored.
      assert(parent == nullptr);
      parent = p;
    }

    void free_region()
    {
      // This must originate from Header::base_dec(true), which is a drop of a
      // register from a frame.
      free_contents();
      delete this;
    }
  };
}
