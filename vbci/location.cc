#include "location.h"

#include "array.h"
#include "header.h"
#include "object.h"
#include "thread.h"

namespace vbci
{
  Region* Location::to_region() const
  {
    if (is_frame_local())
      return Thread::frame_local_region(frame_local_index());

    assert(is_region());
    return reinterpret_cast<Region*>(value);
  }

  template <bool is_move>
  bool drag_allocation(Location dest_loc, Header* h)
  {
    assert(dest_loc.is_region_or_frame_local());

    auto r = dest_loc.to_region();
    bool frame_local = dest_loc.is_frame_local();

    auto& program = Program::get();

    std::vector<Header*> wl;
    std::unordered_map<Header*, RC> rc_map;
    std::unordered_set<Region*> regions;
    wl.push_back(h);

    // Track any borrows to dest_loc, that have now been made internal.
    size_t stack_rc_decs = 0;

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

      if (loc.is_immutable())
        continue;

      // No region, even a frame-local one, can point to the stack.
      if (loc.is_stack())
        return false;

      if (loc.is_frame_local())
      {
        // Younger frames can point to older frames.
        // Older frames and non-frame-local regions drag the object.
        if (frame_local)
        {
          assert(dest_loc.is_frame_local());

          if (dest_loc.frame_local_index() >= loc.frame_local_index())
            continue;
        }

        // This is the first internal edge, give it an internal RC count of 1.
        rc_map[next_h] = 1;

        if (program.is_array(next_h->get_type_id()))
          static_cast<Array*>(next_h)->trace(wl);
        else
          static_cast<Object*>(next_h)->trace(wl);
      }

      // Only need to track regions we visit if the dest_loc is not frame-local
      if (loc.is_region() && !frame_local)
      {
        auto hr = loc.to_region();

        // If hr is r, we do nothing.
        // TODO: Should this do a stack_dec.  We are making a borrow internal?
        if (hr == r)
        {
          stack_rc_decs++;
          continue;
        }

        // If r is not frame-local, it can't point to a region that already has
        // a parent, even if that parent is r (to preserve single entry point).
        if (!frame_local && hr->has_owner())
          return false;

        // If hr is already an ancestor of r, we can't drag the allocation, or
        // we'll create a region cycle.
        if (hr->is_ancestor_of(r))
          return false;

        // If r is not frame-local, it can't have multiple entry points to this
        // region.
        auto [it, ok] = regions.insert(hr);
        if (!frame_local && !ok)
          return false;
      }
    }

    // Assign parent to regions if r is not frame-local.
    if (!frame_local)
    {
      for (auto& hr : regions)
      {
        hr->set_parent(r);
        // Decrease stack rc for this region, as a frame local entry point is
        // now in r.
        hr->stack_dec();
      }
    }

    // Move objects and arrays to the new region.
    for (auto& [hh, rc] : rc_map)
    {
      LOG(Trace) << "Dragging header @" << hh << " to region @" << r << " with internal rc " << rc;
      // Reduce internal RC map by 1 for the initial entry edge.
      if (hh == h)
      {
        if constexpr (!is_move)
        {
          LOG(Trace) << "Copying header @" << hh << " to region @" << r;
          rc--;
        }
        else
          LOG(Trace) << "Moving header @" << hh << " to region @" << r;
      }
      // (hh->rc - rc) = stack rc. This works because frame-local regions are
      // always reference counted.
      assert(hh->get_rc() >= rc);
      r->stack_inc(hh->get_rc() - rc);
      hh->move_region(dest_loc, r);
    }

    if (!frame_local && (stack_rc_decs > 0))
      for (size_t i = 0; i < stack_rc_decs; i++)
        r->stack_dec();

    return true;
    ;
  }

  template bool drag_allocation<false>(Location dest_loc, Header* h);
  template bool drag_allocation<true>(Location dest_loc, Header* h);
}
