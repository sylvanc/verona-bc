#include "freeze.h"

#include "array.h"
#include "header.h"
#include "object.h"
#include "program.h"
#include "region_ext.h"

#include <unordered_set>
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
    return reinterpret_cast<Header*>(
      reinterpret_cast<uintptr_t>(h) & ~uintptr_t(1));
  }

  static bool is_post_order(Header* h)
  {
    return (reinterpret_cast<uintptr_t>(h) & 1) != 0;
  }

  // Union-by-address with rc transfer. Transfers child's external ref count
  // to the new representative, minus 1 for the intra-SCC tree edge.
  // Address as rank + path compression gives O(α(n)) amortized.
  static void scc_union(Header* child, Header* rep)
  {
    auto r = Header::find(child);
    auto rep_r = Header::find(rep);
    assert(r != rep_r);

    // Lower address points to higher address (address serves as rank).
    if (reinterpret_cast<uintptr_t>(r) > reinterpret_cast<uintptr_t>(rep_r))
      std::swap(r, rep_r);

    // Transfer external refs, minus 1 for the intra-SCC tree edge.
    rep_r->set_rc(rep_r->get_rc() + r->get_rc() - 1);
    r->set_location(Location::scc_ptr(rep_r));
  }

  // Trace a header's fields, pushing children onto the DFS stack.
  static void trace_fields(Header* h, std::vector<Header*>& dfs)
  {
    auto& program = Program::get();
    auto fn = [&](Header* h) {
      auto loc = h->location();

      // SCC_PTR — already part of a (potentially incomplete) SCC.
      // Treat as same-region: push to DFS.
      if (loc.is_scc_ptr() || loc.is_pending())
      {
        dfs.push_back(h);
        return;
      }

      // Already immutable or immortal — skip.
      if (loc.is_immutable() || loc.is_immortal())
        return;

      if (loc.is_region())
      {
        dfs.push_back(h);
        return;
      }
    };

    if (program.is_array(h->get_type_id()))
      static_cast<Array*>(h)->trace_fn(fn);
    else
      static_cast<Object*>(h)->trace_fn(fn);
  }

  bool freeze(Header* root)
  {
    assert(root);
    auto loc = root->location();

    if (loc.is_immutable() || loc.is_immortal())
      return true;

    if (loc.is_stack())
      return false;

    auto region = loc.to_region();

    if (region->is_frame_local())
    {
      freeze_local(root);
      return true;
    }

    std::vector<Header*> dfs;
    std::vector<Header*> pending;
    std::unordered_set<Header*> frozen_set;
    RC arc_sum = 0;

    // Freeze a single region's reachable subgraph and adjust its stack_rc.
    // Sub-regions are processed recursively before the parent's stack_rc
    // adjustment, ensuring the parent is alive when children clear ownership.
    auto freeze_region = [&](auto& self, Header* root, bool is_sub) -> void {
      auto region = root->region();

      // Reset per-region state.
      arc_sum = 0;
      frozen_set.clear();
      std::vector<Header*> sub_region_roots;

      // DFS from root.
      dfs.push_back(root);

      while (!dfs.empty())
      {
        Header* h_mark = dfs.back();
        dfs.pop_back();

        if (is_post_order(h_mark))
        {
          Header* h = remove_post_order_mark(h_mark);

          if (!pending.empty() && (pending.back() == h))
          {
            pending.pop_back();
            auto rep = Header::find(h);
            arc_sum += rep->get_rc();
            rep->set_location(Location::immutable());
            rep->set_arc(rep->get_rc());
          }

          continue;
        }

        Header* h = h_mark;
        auto rep = Header::find(h);
        auto rep_loc = rep->location();

        if (rep_loc.is_pending())
        {
          rep->set_rc(rep->get_rc() - 1);

          while (!pending.empty() && pending.back() != rep)
          {
            scc_union(pending.back(), rep);
            pending.pop_back();
          }
        }
        else if (rep_loc.is_immutable())
        {
          // Already frozen. No action needed.
        }
        else if (rep_loc.is_region())
        {
          if (rep_loc.to_region() == region)
          {
            region->remove(h);
            h->set_location(Location::from_raw(Location::Pending));
            frozen_set.insert(h);

            pending.push_back(h);
            dfs.push_back(post_order_mark(h));
            trace_fields(h, dfs);
          }
          else if (!rep_loc.to_region()->is_frame_local())
          {
            // Sub-region entry point. Collect for recursive processing.
            sub_region_roots.push_back(h);
          }
        }
      }

      // Recursively freeze sub-regions BEFORE adjusting the parent's
      // stack_rc. This clears sub-region ownership while the parent
      // region is still alive.
      for (auto sub_root : sub_region_roots)
        self(self, sub_root, true);

      // Count frozen-to-frozen field references.
      RC frozen_internal = 0;
      auto& program = Program::get();

      for (auto fh : frozen_set)
      {
        auto count_fn = [&](Header* target) {
          if (frozen_set.count(target))
            frozen_internal++;
        };

        if (program.is_array(fh->get_type_id()))
          static_cast<Array*>(fh)->trace_fn(count_fn);
        else
          static_cast<Object*>(fh)->trace_fn(count_fn);
      }

      // Count unfrozen-to-frozen field references.
      RC unfrozen_to_frozen = 0;

      region->for_each_header([&](Header* h) {
        auto count_fn = [&](Header* target) {
          if (frozen_set.count(target))
            unfrozen_to_frozen++;
        };

        if (program.is_array(h->get_type_id()))
          static_cast<Array*>(h)->trace_fn(count_fn);
        else
          static_cast<Object*>(h)->trace_fn(count_fn);
      });

      // Adjust stack_rc. For sub-regions, subtract 1 from arc_sum to account
      // for the frozen parent's field ref to the entry point (not a stack ref).
      RC parent_ref = is_sub ? 1 : 0;
      assert(arc_sum >= frozen_internal + unfrozen_to_frozen + parent_ref);
      RC stack_adjustment =
        arc_sum - frozen_internal - unfrozen_to_frozen - parent_ref;

      // Determine ownership clearing before stack_dec.
      bool do_clear_parent = region->has_parent()
        && root == region->get_entry_point();
      bool do_clear_cown = !do_clear_parent && region->has_cown_owner()
        && root == region->get_entry_point();

      for (RC i = 0; i < stack_adjustment; i++)
        region->stack_dec();

      if (do_clear_parent)
        region->clear_parent();
      else if (do_clear_cown)
        region->clear_cown_owner();
    };

    freeze_region(freeze_region, root, false);

    return true;
  }

  void freeze_local(Header* root)
  {
    assert(root);
    assert(root->location().is_region());
    assert(root->location().to_region()->is_frame_local());

    std::vector<Header*> dfs;
    std::vector<Header*> pending;
    std::vector<Header*> heap_roots;

    dfs.push_back(root);

    while (!dfs.empty())
    {
      Header* h_mark = dfs.back();
      dfs.pop_back();

      if (is_post_order(h_mark))
      {
        Header* h = remove_post_order_mark(h_mark);

        if (!pending.empty() && (pending.back() == h))
        {
          pending.pop_back();
          auto rep = Header::find(h);
          rep->set_location(Location::immutable());
          rep->set_arc(rep->get_rc());
        }
        continue;
      }

      Header* h = h_mark;
      auto rep = Header::find(h);
      auto rep_loc = rep->location();

      if (rep_loc.is_pending())
      {
        // Back edge: subtract this intra-SCC edge from rep's rc.
        rep->set_rc(rep->get_rc() - 1);

        // Collapse everything on the pending stack into this SCC.
        while (!pending.empty() && pending.back() != rep)
        {
          scc_union(pending.back(), rep);
          pending.pop_back();
        }
      }
      else if (rep_loc.is_immutable())
      {
        // Cross edge to a completed SCC. Already counted in target's rc.
      }
      else if (rep_loc.is_region())
      {
        auto obj_region = rep_loc.to_region();

        if (!obj_region->is_frame_local())
        {
          // Heap-region object — collect for phase 2 freeze.
          heap_roots.push_back(h);
          continue;
        }

        // Remove from the frame-local region before marking pending.
        obj_region->remove(h);

        h->set_location(Location::from_raw(Location::Pending));

        pending.push_back(h);
        dfs.push_back(post_order_mark(h));

        // Push all region-located children (filtering happens in this loop).
        trace_fields(h, dfs);
      }
    }

    // Phase 2: freeze any heap-region objects discovered during the
    // frame-local DFS. Duplicates and already-frozen objects are handled
    // by freeze() (immutable check returns true early).
    for (auto h : heap_roots)
      freeze(h);
  }
}
