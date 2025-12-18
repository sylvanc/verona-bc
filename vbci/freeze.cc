#include "array.h"
#include "object.h"

namespace vbci
{
  static constexpr auto PostOrder = uintptr_t(0x1);

  static Header* post_order(Header* h)
  {
    return reinterpret_cast<Header*>(
      reinterpret_cast<uintptr_t>(h) | PostOrder);
  }

  static Header* unpost_order(Header* h)
  {
    return reinterpret_cast<Header*>(
      reinterpret_cast<uintptr_t>(h) & ~PostOrder);
  }

  bool freeze(Value& v)
  {
    // This will never encounter a stack allocation.
    auto& program = Program::get();
    std::vector<Region*> regions;
    std::vector<Header*> entry_points;
    std::vector<Header*> dfs;
    std::vector<Header*> pending;

    if (!v.is_header())
      return false;

    auto h = v.get_header();
    auto r = h->region();

    // TODO: could freeze part of a region. Stack RC would no longer be correct.
    // The stack RC can be reset to zero when the behavior terminates, if it
    // hasn't yet.
    // If stack RC > 1, put the region on a "deferred stack RC reset" list?
    // Messes up static type checking of locals. Even worse if the region has a
    // parent, it messes up static type checking fields.
    // Do an extract first instead?
    if (!r || !r->sendable())
      return false;

    entry_points.push_back(h);
    regions.push_back(r);

    while (!entry_points.empty())
    {
      auto ep = entry_points.back();
      entry_points.pop_back();
      r = ep->region();
      dfs.push_back(ep);

      while (!dfs.empty())
      {
        auto h_post = dfs.back();
        dfs.pop_back();
        h = unpost_order(h_post);

        if (h != h_post)
        {
          if (h == pending.back())
          {
            // This is the head of the pending list, so we need to turn it into
            // an SCC.
            pending.pop_back();

            // TODO: how?
          }

          continue;
        }

        auto hloc = h->location();

        if (hloc.is_region() && (h->region() != r))
        {
          // This is a different region, so we need to freeze it.
          // TODO: what if this region has a stack RC?
          entry_points.push_back(h);
          regions.push_back(h->region());
          continue;
        }
        else if (hloc.is_pending())
        {
          // TODO: what if it's already pending?
        }

        // TODO: what if it's already been turned into an SCC?

        // TODO: mark as pending
        r->remove(h);
        pending.push_back(h);
        dfs.push_back(post_order(h));

        if (program.is_array(h->get_type_id()))
          static_cast<Array*>(h)->trace(dfs);
        else
          static_cast<Object*>(h)->trace(dfs);
      }
    }

    // TODO: check for unreachable regions.
    // This is hard if we allow freezing regions with stack RCs.
    // We don't know if the reachable objects have just been frozen or not.
    return true;
  }
}
