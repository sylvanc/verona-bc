#pragma once

#include "classes.h"
#include "ident.h"
#include "types.h"

#include <vbci.h>

namespace vbci
{
  struct Region
  {
  private:
    static constexpr auto CownParent = uintptr_t(0x1);
    Region* parent;
    RC stack_rc;

  protected:
    Region() : parent(nullptr), stack_rc(0) {}
    virtual ~Region() = default;

  public:
    static Region* create(RegionType type);

    virtual Object* object(Class& cls) = 0;
    virtual Array* array(TypeId type_id, size_t size) = 0;
    virtual void rfree(Header* h) = 0;
    virtual void remove(Header* h) = 0;
    virtual bool enable_rc() = 0;

    void stack_inc()
    {
      // If we transition from 0 to 1, we need to increment the parent RC.
      if ((stack_rc++ == 0) && (parent != nullptr))
        parent->stack_inc();
    }

    bool stack_dec()
    {
      if (--stack_rc == 0)
      {
        if (parent == nullptr)
        {
          // Returns false if the region has beeen freed.
          free_region();
          return false;
        }

        // If we transition from 1 to 0, we need to decrement the parent RC.
        parent->stack_dec();
      }

      return true;
    }

    bool is_ancestor(Region* r)
    {
      while (Region* p = r->parent)
      {
        if (p == reinterpret_cast<Region*>(CownParent))
          return false;

        if (p == this)
          return true;

        r = p;
      }

      return false;
    }

    bool sendable()
    {
      assert(stack_rc > 0);
      return (parent == nullptr) && (stack_rc == 1);
    }

    bool has_parent()
    {
      return parent != nullptr;
    }

    void set_parent(Region* r)
    {
      assert(parent == nullptr);
      assert(stack_rc > 0);
      parent = r;
      parent->stack_inc();
    }

    void set_parent()
    {
      assert(parent == nullptr);
      assert(stack_rc > 0);
      parent = reinterpret_cast<Region*>(CownParent);
    }

    void clear_parent()
    {
      assert(parent != nullptr);
      assert(stack_rc > 0);

      if (parent != reinterpret_cast<Region*>(CownParent))
        parent->stack_dec();

      parent = nullptr;
    }

  private:
    virtual void free_contents() = 0;

    void free_region()
    {
      // This must originate from Header::base_dec(true), which is a drop of a
      // register from a frame.
      free_contents();
      delete this;
    }
  };
}
