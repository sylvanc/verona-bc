#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace vbci
{
  struct Stack
  {
    struct Idx
    {
      size_t chunk;
      size_t offset;
    };

    static constexpr size_t ChunkSize = 1024;
    using Chunk = std::array<uint8_t, ChunkSize>;
    std::vector<Chunk> chunks;
    Idx top;

    Idx save()
    {
      return top;
    }

    void restore(const Idx& idx)
    {
      top = idx;
    }

    void* alloc(size_t size)
    {
      if (size > ChunkSize)
        return nullptr;

      if ((top.offset + size) > ChunkSize)
      {
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
  };
}
