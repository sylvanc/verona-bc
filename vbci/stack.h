#pragma once

#include "header.h"
#include "ident.h"

#include <array>
#include <cstdint>
#include <functional>
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

  public:
    Idx save();
    void restore(const Idx& idx);
    void* alloc(size_t size);
    Array* array(Location frame_id, uint32_t type_id, size_t size);

    void visit_headers(
      Idx start,
      Idx end,
      const std::function<void(Header*)>& fn);
  };
}
