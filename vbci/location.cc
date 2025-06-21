#include "location.h"

#include "array.h"
#include "header.h"
#include "object.h"

namespace vbci
{
  bool drag_allocation(Region* r, Header* h)
  {
    Location frame = loc::None;

    if (r->is_frame_local())
      frame = r->get_parent();

    std::vector<Header*> wl;
    std::unordered_map<Header*, RC> rc_map;
    std::unordered_set<Region*> regions;
    wl.push_back(h);

    while (!wl.empty())
    {
      h = wl.back();
      wl.pop_back();
      auto find = rc_map.find(h);

      if (find != rc_map.end())
      {
        // If we're already tracked, increase the internal RC.
        find->second++;
        continue;
      }

      auto loc = h->location();

      if (loc::is_immutable(loc))
        continue;

      // No region, even a frame-local one, can point to the stack.
      if (loc::is_stack(loc))
        return false;

      auto hr = loc::to_region(loc);

      if (hr->is_frame_local())
      {
        // Younger frames can point to older frames.
        // Older frames and non-frame-local regions drag the object.
        if (frame >= hr->get_parent())
          continue;

        // Initial internal RC count is 1.
        rc_map[h] = 1;

        if (h->is_array())
          static_cast<Array*>(h)->trace(wl);
        else
          static_cast<Object*>(h)->trace(wl);
      }
      else
      {
        // If hr is r, or hr's parent is r, we do nothing.
        if ((hr == r) || (hr->get_parent() == Location(r)))
          continue;

        // If r is not frame-local, it can't point to a region that already has
        // a parent.
        if ((frame == loc::None) && hr->has_parent())
          return false;

        // If hr is already an ancestor of r, we can't drag the allocation.
        if (hr->is_ancestor_of(r))
          return false;

        // If r is not frame-local, it can't have multiple entry points to this
        // region.
        auto [it, ok] = regions.insert(hr);
        if ((frame == loc::None) && !ok)
          return false;
      }
    }

    // Reparent regions.
    for (auto& hr : regions)
      hr->set_parent(r);

    // Move objects and arrays to the new region.
    for (auto& [hh, rc] : rc_map)
    {
      // If h == hh, reduce internal RC map by 1 for the initial entry edge.
      if (h == hh)
        rc--;

      // (hh->rc - rc) = stack rc. This works because frame-local regions are
      // always reference counted.
      assert(hh->get_rc() >= rc);
      r->stack_inc(hh->get_rc() - rc);
      hh->move_region(r);
    }

    return true;
  }
}
