#include "../lang.h"

namespace vc
{
  int lower_once(
    Node top, const OnceFunctions& once_funcs, const CallGraph& call_graph)
  {
    if (once_funcs.empty())
      return 0;

    enum class Phase
    {
      Pre,
      Post
    };

    struct Work
    {
      Phase phase;
      Location id;
      Node site;
    };

    struct PendingOnce
    {
      Location id;
      size_t depth;
    };

    std::set<Location> complete;
    std::map<Location, size_t> pending;
    std::vector<PendingOnce> pending_once;
    std::vector<Work> work;

    for (auto& root : once_funcs)
      work.push_back({Phase::Pre, root, {}});

    while (!work.empty())
    {
      auto [phase, id, incoming_site] = std::move(work.back());
      work.pop_back();

      if (phase == Phase::Post)
      {
        if (once_funcs.count(id))
        {
          assert(pending_once.back().id == id);
          pending_once.pop_back();

          auto id_str = std::string(id.view());
          top
            << (Memo << (MemoId ^ (id_str + "$slot"))
                     << (FunctionId ^ (id_str + "$once")));
        }

        pending.erase(id);
        complete.insert(id);
        continue;
      }

      if (complete.count(id))
        continue;

      auto pending_it = pending.find(id);
      if (pending_it != pending.end())
      {
        if (
          pending_once.empty() ||
          pending_once.back().depth < pending_it->second)
          continue;

        Node error = Error << errmsg("once functions form a cycle");
        bool error_path = false;

        assert(incoming_site);

        for (auto& item : work)
        {
          if (item.phase != Phase::Post)
            continue;

          if (item.id == id)
          {
            error_path = true;
            continue;
          }

          if (error_path)
            error << errloc(item.site);
        }

        error << errloc(incoming_site);
        top << error;
        return 1;
      }

      auto depth = pending.size();
      pending.emplace(id, depth);

      if (once_funcs.count(id))
        pending_once.push_back({id, depth});

      work.push_back({Phase::Post, id, incoming_site});

      auto calls = call_graph.find(id);
      if (calls != call_graph.end())
      {
        for (auto it = calls->second.rbegin(); it != calls->second.rend(); ++it)
          work.push_back({Phase::Pre, it->first, it->second});
      }
    }

    return 0;
  }
}
