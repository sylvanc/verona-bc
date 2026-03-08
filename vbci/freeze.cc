#include "freeze.h"

#include "array.h"
#include "header.h"
#include "object.h"
#include "program.h"
#include "region.h"

#include <vector>

namespace vbci
{
  // Post-order marker: set LSB of pointer.
  static Header* post_order_mark(Header* h)
  {
    return reinterpret_cast<Header*>(reinterpret_cast<uintptr_t>(h) | 1);
  }

  static Header* remove_post_order_mark(Header* h)
  {
    return reinterpret_cast<Header*>(reinterpret_cast<uintptr_t>(h) & ~uintptr_t(1));
  }

  static bool is_post_order(Header* h)
  {
    return (reinterpret_cast<uintptr_t>(h) & 1) != 0;
  }

  // Get rank from a PENDING header (stored in rc field).
  static RC get_rank(Header* h)
  {
    assert(h->location().is_pending());
    return h->get_rc();
  }

  // Union two SCC representatives. Returns false if already same SCC.
  // Uses rank-balanced union.
  static bool scc_union(Header* h1, Header* h2)
  {
    auto r1 = Header::find(h1);
    auto r2 = Header::find(h2);

    if (r1 == r2)
      return false;

    auto rank1 = get_rank(r1);
    auto rank2 = get_rank(r2);

    if (rank1 > rank2)
      std::swap(r1, r2);
    else if (rank1 == rank2)
      r2->set_rc(rank2 + 1);

    // r1 (smaller rank) points to r2 (larger rank).
    r1->set_location(Location::scc_ptr(r2));
    return true;
  }

  // Trace a header's fields, pushing region-object children onto the DFS stack.
  // Returns external references (cowns, immutables) that need incref.
  static void trace_fields(
    Header* h, std::vector<Header*>& dfs, Region* source_region)
  {
    auto& program = Program::get();
    std::vector<Header*> refs;

    if (program.is_array(h->get_type_id()))
      static_cast<Array*>(h)->trace_all(refs);
    else
      static_cast<Object*>(h)->trace_all(refs);

    for (auto child : refs)
    {
      if (!child)
        continue;

      auto child_loc = child->location();

      // Already immutable or immortal — skip.
      if (child_loc.is_immutable() || child_loc.is_immortal())
        continue;

      // SCC_PTR — already part of a (potentially incomplete) SCC.
      // Treat as same-region: push to DFS.
      if (child_loc.is_scc_ptr() || child_loc.is_pending())
      {
        dfs.push_back(child);
        continue;
      }

      // In the source region — process via DFS.
      if (child_loc.is_region() && child_loc.to_region() == source_region)
      {
        dfs.push_back(child);
        continue;
      }

      // External region object — this shouldn't happen for isolated regions.
      // For now, skip (the freeze caller must ensure isolation).
    }
  }

  void freeze(Region* region, Header* root)
  {
    assert(root);
    auto all_headers = region->get_headers();

    std::vector<Header*> dfs;
    std::vector<Header*> pending;

    // Start DFS from the root.
    dfs.push_back(root);

    while (!dfs.empty())
    {
      Header* h_mark = dfs.back();
      dfs.pop_back();

      if (is_post_order(h_mark))
      {
        // Post-order visit: check if this completes an SCC.
        Header* h = remove_post_order_mark(h_mark);

        if (!pending.empty() && pending.back() == h)
        {
          pending.pop_back();
          // This is the SCC root. Mark as frozen with RC=1.
          auto rep = Header::find(h);
          rep->set_location(Location::immutable());
          rep->set_arc(1);
        }
        continue;
      }

      Header* h = h_mark;
      auto rep = Header::find(h);
      auto rep_loc = rep->location();

      if (rep_loc.is_pending())
      {
        // Back edge: collapse everything on the path to this SCC.
        while (!pending.empty() &&
               Header::find(pending.back()) != rep)
        {
          scc_union(pending.back(), rep);
          pending.pop_back();
        }
      }
      else if (rep_loc.is_immutable())
      {
        // Cross edge to a completed SCC. Increment its ARC.
        rep->inc_arc();
      }
      else if (rep_loc.is_region())
      {
        // UNMARKED: first visit.
        h->set_location(Location::from_raw(Location::Pending));
        h->set_rc(0); // rank = 0

        pending.push_back(h);
        dfs.push_back(post_order_mark(h));

        // Trace fields and push children.
        trace_fields(h, dfs, region);
      }
      // SCC_PTR with non-pending target — already processed, skip.
    }

    // Sweep: finalize and free unreachable objects.
    auto& program = Program::get();

    for (auto h : all_headers)
    {
      auto loc = h->location();

      if (loc.is_region() && loc.to_region() == region)
      {
        // Still UNMARKED = unreachable from root. Finalize and free.
        if (program.is_array(h->get_type_id()))
          static_cast<Array*>(h)->finalize();
        else
          static_cast<Object*>(h)->finalize();

        delete[] reinterpret_cast<uint8_t*>(h);
      }
    }

    // Delete the region (all surviving objects are now frozen).
    delete region;
  }
}
