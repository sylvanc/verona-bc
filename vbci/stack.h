#pragma once

#include "header.h"
#include "ident.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace vbci
{
  struct Stack
  {
    friend struct Thread;

    struct Idx
    {
      size_t chunk;
      size_t offset;
    };

  private:
    static constexpr size_t ChunkSize = 1024;
    static constexpr size_t Align = 8;
    static constexpr size_t align_up(size_t n)
    {
      return (n + (Align - 1)) & ~(Align - 1);
    }
    using Chunk = std::array<uint8_t, ChunkSize>;
    std::vector<std::unique_ptr<Chunk>> chunks;
    Idx top;

    static size_t size_bytes(Header* h);

  public:
    Idx save();
    void restore(const Idx& idx);
    void* alloc(size_t size);
    Array* array(Location frame_id, uint32_t type_id, size_t size);

    template<typename Fn>
    void visit_headers(Idx start, Idx end, Fn&& fn)
    {
      for (size_t c = start.chunk; c <= end.chunk && c < chunks.size(); c++)
      {
        auto offset = (c == start.chunk) ? start.offset : 0;
        auto limit = (c == end.chunk) ? end.offset : ChunkSize;

        if (offset >= limit)
          continue;

        auto& chunk = *chunks.at(c);

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
          // Walker steps match the allocator's 8-byte alignment so we skip any
          // padding inserted at allocation time.
          offset += align_up(size_bytes(h));
        }
      }
    }
  };
}
