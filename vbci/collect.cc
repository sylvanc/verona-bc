/**
 * Implements a worklist collector for applying teardown code for objects,
 * regions, and arrays. Uses thread local storage to maintain the worklist, so
 * that reentrancy can be delayed until each item has completed its teardown.
 */

#include "array.h"
#include "object.h"
#include "region.h"

#include <vbci.h>
#include <queue>

namespace vbci
{
  enum class CollectorType
  {
    Region,
    Header
  };

  // TODO: This could be optimised by borrowing low bits from the pointer if
  // alignment allows it.  Only do this if profiling shows this is a bottleneck.
  struct WorkItem
  {
    CollectorType type;
    void* header;

    WorkItem(CollectorType t, void* h) : type(t), header(h) {}
  };

  // Used to get tag from type to reduce code duplication.
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
  static thread_local bool in_collection = false;

  template<typename T>
  static bool add_work_list(T* h)
  {
    if (!in_collection)
      return false;

    LOG(Trace) << "Adding to worklist: " << static_cast<void*>(h);
    worklist.emplace(Tag<T>::value, h);
    return true;
  }

  template<typename T>
  void collect(T* h)
  {
    if (add_work_list(h))
      return;


    in_collection = true;
    add_work_list(h);
    auto& program = Program::get();

    while (!worklist.empty())
    {
      auto n = worklist.front();
      worklist.pop();

      LOG(Trace) << "Processing work item: " << static_cast<void*>(n.header);
      switch (n.type)
      {
        case CollectorType::Header:
        {
          auto h = static_cast<Header*>(n.header);
          if (program.is_array(h->get_type_id()))
            static_cast<Array*>(h)->deallocate();
          else
            static_cast<Object*>(h)->deallocate();
          break;
        }

        case CollectorType::Region:
          static_cast<Region*>(n.header)->deallocate();
          break;
      }
    }
    in_collection = false;
  }

  template void collect<Header>(Header* h);
  template void collect<Region>(Region* h);
} // namespace vbci