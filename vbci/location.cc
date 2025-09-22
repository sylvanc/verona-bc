#include "location.h"

#include "array.h"
#include "header.h"
#include "object.h"

namespace vbci
{
  std::pair<bool, bool> drag_allocation(Region* r, Header* h, Location ploc)
  {
    bool pr_rc = 0;
    Location frame = loc::None;

    if (r->is_frame_local())
      frame = r->get_parent();

    std::vector<Header*> wl;
    std::unordered_map<Header*, RC> rc_map;
    std::unordered_set<Region*> regions;
    wl.push_back(h);

    while (!wl.empty())
    {
      Header* next_h = wl.back();
      wl.pop_back();
      auto find = rc_map.find(next_h);

      if (find != rc_map.end())
      {
        // If we're already tracked, increase the internal RC.
        find->second++;
        continue;
      }

      auto loc = next_h->location();

      if (loc::is_immutable(loc))
        continue;

      // No region, even a frame-local one, can point to the stack.
      if (loc::is_stack(loc))
        return std::pair(false, false);

      auto hr = loc::to_region(loc);

      if (hr->is_frame_local())
      {
        // Younger frames can point to older frames.
        // Older frames and non-frame-local regions drag the object.
        if (frame >= hr->get_parent())
          continue;

        // Initial internal RC count is 1.
        rc_map[next_h] = 1;

        if (next_h->is_array())
          static_cast<Array*>(next_h)->trace(wl);
        else
          static_cast<Object*>(next_h)->trace(wl);
      }
      else
      {
        // If hr is r, we do nothing.
        if (hr == r)
          continue;

        // If r is not frame-local, it can't point to a region that already has
        // a parent, even if that parent is r (to preserve single entry point).
        if ((frame == loc::None) && hr->has_parent())
        {
          // if hr is the previous region, then as long as there is only one
          // reference into hr from frame locals we are dragging into r, it's
          // ok. (this reference will replace the old one from r still leaving
          // only one entry point to hr)
          if (loc::is_region(ploc) && loc::to_region(ploc) == hr)
          {
            if (pr_rc >= 1)
              return std::pair(false, false);
            pr_rc += 1;
          }
          else
            return std::pair(false, false);
        }

        // If hr is already an ancestor of r, we can't drag the allocation, or
        // we'll create a region cycle.
        if (hr->is_ancestor_of(r))
          return std::pair(false, false);

        // If r is not frame-local, it can't have multiple entry points to this
        // region.
        auto [it, ok] = regions.insert(hr);
        if ((frame == loc::None) && !ok)
          return std::pair(false, false);
      }
    }

    // Assign parent to regions if r is not frame-local.
    if (frame == loc::None)
    {
      for (auto& hr : regions)
      {
        if (loc::is_region(ploc) && loc::to_region(ploc) == hr && pr_rc > 0)
        {
          // OK
        }

        else
          hr->set_parent(r);
        // Decrease stack rc for this region, as a frame local entry point is
        // now in r.
        hr->stack_dec();
      }
    }

    // Move objects and arrays to the new region.
    for (auto& [hh, rc] : rc_map)
    {
      // Reduce internal RC map by 1 for the initial entry edge.
      if (hh == h)
        rc--;

      // (hh->rc - rc) = stack rc. This works because frame-local regions are
      // always reference counted.
      assert(hh->get_rc() >= rc);
      r->stack_inc(hh->get_rc() - rc);
      hh->move_region(r);
    }

    return std::pair(true, pr_rc == 0);
  }
}
