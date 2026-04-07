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
  bool drag_allocation(Region* r, Header* h, Region** pr)
  {
    bool frame_local = r->is_frame_local();
    auto& program = Program::get();
    size_t stack_rc_decs = 0;

    std::vector<Header*> wl;
    std::unordered_map<Header*, RC> rc_map;
    std::unordered_map<Region*, Header*> regions;
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

      if (!hr->is_frame_local())
      {
        // Object is in a heap region — already parented within its own
        // region tree. Sub-regions of hr are parented to hr, not to us.
        // No dragging, no sub-region tracking needed.

        // If this is the destination for a write-barrier exchange, clear
        // the exchange pointer so apply_out doesn't unparent it.
        if (pr && (*pr == hr))
        {
          *pr = nullptr;
          hr->stack_dec();
        }
        else if (!frame_local)
        {
          // Heap-to-heap drag: parent this region to the destination.
          if (hr->has_parent() && (hr->get_parent() == r))
            continue;

          if (hr->has_owner())
            return false;

          if (hr->is_ancestor_of(r))
            return false;

          if (!regions.emplace(hr, next_h).second)
            return false;
        }

        continue;
      }

      // Object is in a frame-local region — drag it to the destination.
      rc_map[next_h] = 1;

      if (program.is_array(next_h->get_type_id()))
        static_cast<Array*>(next_h)->trace_fn(fn);
      else
        static_cast<Object*>(next_h)->trace_fn(fn);
    }

    // Guard the destination region against premature free during sub-region
    // reparenting. Sub-region stack_dec can cascade to the destination (via the
    // newly set parent pointer) before the dragged objects have contributed
    // their stack_rc.
    r->stack_inc();

    // Parent tracked sub-regions to the destination.
    for (auto& [hr, entry] : regions)
    {
      hr->set_parent(r, entry);
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

    r->stack_dec(stack_rc_decs);

    // Release the guard.
    r->stack_dec();
    return true;
  }

  template bool drag_allocation<false>(Region* dest, Header* h, Region** pr);
  template bool drag_allocation<true>(Region* dest, Header* h, Region** pr);
}
