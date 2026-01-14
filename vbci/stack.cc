#include "stack.h"

#include "array.h"
#include "header.h"
#include "object.h"
#include "program.h"
 namespace vbci
{
  Stack::Idx Stack::save()
  {
    return top;
  }

  void Stack::restore(const Idx& idx)
  {
    top = idx;
  }

  void* Stack::alloc(size_t size)
  {
    if (size > ChunkSize)
      return nullptr;

    if ((top.offset + size) > ChunkSize)
    {
      struct StackMarker : Header
      {
        StackMarker() : Header(Location::stack(), StackSentinelTypeId) {}
      };

      // When this allocation would overflow the current chunk, drop a marker
      // so walkers know they should stop in this chunk; if no room remains for
      // the marker, just leave the tail unused.
      auto remaining = ChunkSize - top.offset;

      if (remaining >= sizeof(StackMarker))
      {
        auto* marker = reinterpret_cast<StackMarker*>(
          chunks.at(top.chunk).data() + top.offset);
        new (marker) StackMarker();
      }

      top.chunk++;
      top.offset = 0;
    }

    if (top.chunk >= chunks.size())
    {
      chunks.emplace_back();
      top.chunk = chunks.size() - 1;
      top.offset = 0;
    }

    auto ret = &chunks.at(top.chunk).at(top.offset);
    top.offset += size;
    return ret;
  }

  Array* Stack::array(Location frame_id, uint32_t type_id, size_t size)
  {
    auto& program = Program::get();
    auto content_type_id = program.unarray(type_id);
    auto rep = program.layout_type_id(content_type_id);
    auto mem = alloc(Array::size_of(size, rep.second->size));
    return Array::create(
      mem, frame_id, type_id, rep.first, size, rep.second->size);
  }

  void Stack::visit_headers(
    Idx start, Idx end, const std::function<void(Header*)>& fn)
  {
    auto size_bytes = [&](Header* h) -> size_t {
      if (Program::get().is_array(h->get_type_id()))
        return static_cast<Array*>(h)->allocation_size_bytes();
      return static_cast<Object*>(h)->allocation_size_bytes();
    };

    for (size_t c = start.chunk; c <= end.chunk && c < chunks.size(); c++)
    {
      auto offset = (c == start.chunk) ? start.offset : 0;
      auto limit = (c == end.chunk) ? end.offset : ChunkSize;

      if (offset >= limit)
        continue;

      auto& chunk = chunks.at(c);

      while (offset < limit)
      {
        auto remaining = limit - offset;

        // Not enough space for even a header; nothing more to read here.
        if (remaining < sizeof(Header))
          break;

        auto* h = reinterpret_cast<Header*>(chunk.data() + offset);

        assert(h != nullptr);
        assert(h->location().is_stack());

        // Sentinel marks end of live allocations in this chunk.
        if (h->get_type_id() == Header::StackSentinelTypeId)
          break;

        fn(h);
        offset += size_bytes(h);
      }
    }
  }
}
