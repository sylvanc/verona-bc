#include "location.h"

#include "array.h"
#include "header.h"
#include "object.h"
#include "thread.h"

namespace vbci
{
  Region* Location::to_region() const
  {
    assert(is_region());
    return reinterpret_cast<Region*>(value);
  }

  template<bool is_move>
  bool drag_allocation(Region* r, Header* h, Region* ignore_parent)
  {
    bool frame_local = r->is_frame_local();
    auto& program = Program::get();
    size_t stack_rc_decs = 0;

    std::vector<Header*> wl;
    std::unordered_map<Header*, RC> rc_map;
    std::unordered_set<Region*> regions;
    wl.push_back(h);

    auto fn = [&](Header* h) {
      // Only add mutable, heap allocated objects and arrays to the list.
      if (h->location().is_region())
        wl.push_back(h);
    };

    while (!wl.empty())
    {
      Header* next_h = wl.back();
      wl.pop_back();

      auto find = rc_map.find(next_h);
      if (find != rc_map.end())
      {
        find->second++;
        continue;
      }

      auto loc = next_h->location();

      if (loc.is_immutable())
        continue;

      if (loc.is_stack())
        return false;

      assert(loc.is_region());
      auto hr = loc.to_region();

      // Already in the destination region — just track the internal borrow.
      if (hr == r)
      {
        if (!frame_local)
          stack_rc_decs++;
        continue;
      }

      // Frame-local dest: older frame-local regions outlive this one, skip.
      if (
        frame_local && hr->is_frame_local() &&
        r->get_frame_depth() >= hr->get_frame_depth())
        continue;

      if (hr->is_frame_local())
      {
        // Object is in a frame-local region — drag it to the destination.
        rc_map[next_h] = 1;

        if (program.is_array(next_h->get_type_id()))
          static_cast<Array*>(next_h)->trace_fn(fn);
        else
          static_cast<Object*>(next_h)->trace_fn(fn);

        // Frame-local dest: no sub-region tracking needed.
        if (frame_local)
          continue;
      }
      else
      {
        // Object is in a different heap region — don't drag it, but
        // trace its fields to discover sub-sub-regions.
        if (program.is_array(next_h->get_type_id()))
          static_cast<Array*>(next_h)->trace_fn(fn);
        else
          static_cast<Object*>(next_h)->trace_fn(fn);
      }

      // Sub-region tracking for heap regions.
      if (frame_local)
        continue;

      // Frame-local source objects are dragged, not sub-region tracked.
      if (hr->is_frame_local())
        continue;

      // Sub-region already parented to the destination — no new entry.
      if (hr->has_parent() && hr->get_parent() == r)
        continue;

      // Sub-region's parent is about to be cleared by the exchange.
      if (
        ignore_parent && hr->has_parent() && hr->get_parent() == ignore_parent)
        continue;

      // Sub-region has an owner we can't clear — single entry violated.
      if (hr->has_owner())
        return false;

      // Would create a cycle.
      if (hr->is_ancestor_of(r))
        return false;

      // Can't have multiple entry points to the same sub-region.
      if (!regions.insert(hr).second)
        return false;
    }

    // Parent tracked sub-regions to the destination.
    for (auto& hr : regions)
    {
      hr->set_parent(r);
      hr->stack_dec();
    }

    // Move objects to the new region.
    for (auto& [hh, rc] : rc_map)
    {
      LOG(Trace) << "Dragging header @" << hh << " to region @" << r
                 << " with internal rc " << rc;

      if (hh == h)
      {
        if constexpr (!is_move)
          rc--;
      }

      assert(hh->get_rc() >= rc);
      r->stack_inc(hh->get_rc() - rc);
      hh->move_region(r);
    }

    for (size_t i = 0; i < stack_rc_decs; i++)
      r->stack_dec();

    return true;
  }

  template bool
  drag_allocation<false>(Region* dest, Header* h, Region* ignore_parent);
  template bool
  drag_allocation<true>(Region* dest, Header* h, Region* ignore_parent);
}
