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
    Object,
    Region,
    Array
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
  struct Tag<Object>
  {
    static constexpr CollectorType value = CollectorType::Object;
  };

  template<>
  struct Tag<Region>
  {
    static constexpr CollectorType value = CollectorType::Region;
  };

  template<>
  struct Tag<Array>
  {
    static constexpr CollectorType value = CollectorType::Array;
  };

  static thread_local std::queue<WorkItem> worklist;
  static thread_local bool in_collection = false;

  template<typename T>
  static bool add_work_list(T* h)
  {
    if (!in_collection)
      return false;

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

    while (!worklist.empty())
    {
      auto n = worklist.front();
      worklist.pop();

      switch (n.type)
      {
        case CollectorType::Object:
          static_cast<Object*>(n.header)->deallocate();
          break;

        case CollectorType::Region:
          static_cast<Region*>(n.header)->deallocate();
          break;

        case CollectorType::Array:
          static_cast<Array*>(n.header)->deallocate();
          break;
      }
    }
    in_collection = false;
  }

  template void collect<Object>(Object* h);
  template void collect<Region>(Region* h);
  template void collect<Array>(Array* h);
} // namespace vbci