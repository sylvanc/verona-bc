#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace vbcc
{
  struct Bitset
  {
    static constexpr auto Bits = std::numeric_limits<uint64_t>::digits;
    static constexpr size_t idx(size_t i);
    static constexpr size_t off(size_t i);

  private:
    std::vector<uint64_t> bits;

  public:
    Bitset(size_t size = 0);

    operator bool() const;
    bool empty() const;
    bool unsized() const;

    void resize(size_t size);
    bool test(size_t i) const;
    void set(size_t i);
    void reset(size_t i);

    Bitset operator~() const;
    Bitset operator&(const Bitset& that) const;
    Bitset operator|(const Bitset& that) const;

    Bitset& operator&=(const Bitset& that);
    Bitset& operator|=(const Bitset& that);

    bool operator==(const Bitset& that) const;
    bool operator!=(const Bitset& that) const;
  };
}
