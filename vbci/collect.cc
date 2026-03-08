/**
 * Implements a two-phase collector for objects, regions, and arrays.
 *
 * Phase 1 (finalizing): Run finalizers and drop field references. This may
 * cause other objects' RCs to hit 0, adding them to the finalizing worklist.
 * Drain until empty.
 *
 * Phase 2 (deleting): Free memory for all finalized objects.
 *
 * This separation ensures that all finalizers run before any memory is freed,
 * preventing use-after-free when finalizers reference sibling objects.
 */

#include "array.h"
#include "object.h"
#include "region.h"

#include <queue>
#include <vector>
#include <vbci.h>

namespace vbci
{
  enum class CollectorType
  {
    Region,
    Header
  };

  struct WorkItem
  {
    CollectorType type;
    void* header;

    WorkItem(CollectorType t, void* h) : type(t), header(h) {}
  };

  template<typename T>
  struct Tag;

  template<>
  struct Tag<Header>
  {
    static constexpr CollectorType value = CollectorType::Header;
  };

  template<>
  struct Tag<Region>
  {
    static constexpr CollectorType value = CollectorType::Region;
  };

  static thread_local std::queue<WorkItem> worklist;
  static thread_local std::vector<WorkItem> to_delete;
  static thread_local bool in_collection = false;

  static void drain_work_list()
  {
    auto& program = Program::get();
    in_collection = true;

    // Phase 1: Finalize all objects, collecting the transitive closure.
    while (!worklist.empty())
    {
      auto n = worklist.front();
      worklist.pop();
      LOG(Trace) << "Finalizing work item: " << static_cast<void*>(n.header);

      switch (n.type)
      {
        case CollectorType::Header:
        {
          auto h = static_cast<Header*>(n.header);

          if (program.is_array(h->get_type_id()))
            static_cast<Array*>(h)->finalize();
          else
            static_cast<Object*>(h)->finalize();
          break;
        }

        case CollectorType::Region:
        {
          auto r = static_cast<Region*>(n.header);
          r->finalize_contents();
          break;
        }
      }

      to_delete.push_back(n);
    }

    // Phase 2: Delete all finalized objects.
    for (auto& n : to_delete)
    {
      LOG(Trace) << "Deleting work item: " << static_cast<void*>(n.header);

      switch (n.type)
      {
        case CollectorType::Header:
        {
          auto h = static_cast<Header*>(n.header);

          if (h->location().is_immutable())
            delete[] reinterpret_cast<uint8_t*>(h);
          else
            h->region()->rfree(h);
          break;
        }

        case CollectorType::Region:
        {
          auto r = static_cast<Region*>(n.header);
          r->release_dead_objects();
          break;
        }
      }
    }

    to_delete.clear();
    in_collection = false;
  }

  template<typename T>
  void collect(T* h)
  {
    worklist.emplace(Tag<T>::value, h);

    if (!in_collection)
      drain_work_list();
  }

  template void collect<Header>(Header* h);
  template void collect<Region>(Region* h);
} // namespace vbci
