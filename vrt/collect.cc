#include "collect.h"

#include "failure.h"
#include "header.h"
#include "region.h"
#include "region_ext.h"

#include <deque>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace vrt
{
  namespace
  {
    struct HeaderWork
    {
      Header* header;
      Region* owner_guard;
    };

    struct RegionWork
    {
      Region* region;
    };

    using WorkItem = std::variant<HeaderWork, RegionWork>;

    struct CollectorState
    {
      std::deque<WorkItem> pending_work;

      // Finalized work waits here until pending_work is empty, preserving the
      // finalize-before-reclaim phase boundary.
      std::vector<HeaderWork> pending_header_releases;
      std::vector<RegionWork> pending_region_releases;
      bool draining = false;
    };

    thread_local CollectorState collector_state;

    void drain()
    {
      internal_check(
        !collector_state.draining, Failure::invalid_header_state);
      collector_state.draining = true;

      while (true)
      {
        while (!collector_state.pending_work.empty())
        {
          auto item = std::move(collector_state.pending_work.front());
          collector_state.pending_work.pop_front();

          if (auto* header = std::get_if<HeaderWork>(&item))
          {
            header->header->finalize();
            collector_state.pending_header_releases.push_back(*header);
          }
          else
          {
            const auto region = std::get<RegionWork>(item);
            region.region->finalize_contents();
            collector_state.pending_region_releases.push_back(region);
          }
        }

        auto header_releases =
          std::move(collector_state.pending_header_releases);
        collector_state.pending_header_releases.clear();
        for (const auto& header_release : header_releases)
        {
          header_release.header->destroy_storage();
          if (header_release.owner_guard != nullptr)
            header_release.owner_guard->stack_dec();
        }

        auto region_releases =
          std::move(collector_state.pending_region_releases);
        collector_state.pending_region_releases.clear();
        for (const auto& region_release : region_releases)
          region_release.region->release_dead_objects();

        if (collector_state.pending_work.empty())
          break;
      }

      collector_state.draining = false;
    }

    void start_drain()
    {
      if (!collector_state.draining)
        drain();
    }
  }

  void collect(Header* header)
  {
    if (
      (header == nullptr) || header->location().is_immortal() ||
      header->finalizing)
      return;

    auto* region = header->region();
    if (
      (region == nullptr) || region->destroying || region->is_finalizing() ||
      region->is_arena())
      return;

    region->stack_inc();
    if (!region->remove(header))
    {
      region->stack_dec();
      return;
    }

    collector_state.pending_work.emplace_back(HeaderWork{header, region});
    start_drain();
  }

  void collect(Region* region)
  {
    if ((region == nullptr) || region->destroying)
      return;

    internal_check(region->parent == nullptr, Failure::invalid_region_state);

    region->destroying = true;
    const bool began_finalizing = region->begin_finalizing();
    internal_check(began_finalizing, Failure::invalid_region_state);

    collector_state.pending_work.emplace_back(RegionWork{region});
    start_drain();
  }

  void collect_scc(Header* representative)
  {
    internal_check(
      (representative != nullptr) &&
        (representative->location() == Location::immutable()) &&
        (representative->representative() == representative) &&
        (representative->get_arc() == 0),
      Failure::invalid_header_state);

    if (!representative->try_begin_scc_collection())
      return;

    std::vector<Header*> work{representative};
    std::unordered_set<Header*> members;
    members.emplace(representative);

    while (!work.empty())
    {
      auto* header = work.back();
      work.pop_back();

      header->trace_fn([&](Header* child) {
        if (
          child->location().is_immutable() &&
          (child->representative() == representative) &&
          members.emplace(child).second)
          work.push_back(child);
      });
    }

    for (auto* member : members)
      collector_state.pending_work.emplace_back(HeaderWork{member, nullptr});

    start_drain();
  }
}