#include "bitset.h"

namespace vbcc
{
  constexpr size_t Bitset::idx(size_t i)
  {
    return i / Bits;
  }

  constexpr size_t Bitset::off(size_t i)
  {
    return i % Bits;
  }

  Bitset::Bitset(size_t size)
  {
    if (size > 0)
      resize(size);
  }

  Bitset::operator bool() const
  {
    return !empty();
  }

  bool Bitset::empty() const
  {
    for (const auto& bit : bits)
    {
      if (bit != 0)
        return false;
    }

    return true;
  }

  void Bitset::resize(size_t size)
  {
    bits.resize(idx(size - 1) + 1);
  }

  bool Bitset::test(size_t i) const
  {
    return (bits.at(idx(i)) >> off(i)) & 1;
  }

  void Bitset::set(size_t i)
  {
    bits.at(idx(i)) |= (uint64_t(1) << off(i));
  }

  void Bitset::reset(size_t i)
  {
    bits.at(idx(i)) &= ~(uint64_t(1) << off(i));
  }

  Bitset Bitset::operator~() const
  {
    Bitset result = *this;

    for (size_t i = 0; i < bits.size(); i++)
      result.bits.at(i) = ~bits.at(i);

    return result;
  }

  Bitset Bitset::operator&(const Bitset& that) const
  {
    Bitset result = *this;
    result &= that;
    return result;
  }

  Bitset Bitset::operator|(const Bitset& that) const
  {
    Bitset result = *this;
    result |= that;
    return result;
  }

  Bitset& Bitset::operator&=(const Bitset& that)
  {
    for (size_t i = 0; i < bits.size(); i++)
      bits.at(i) &= that.bits.at(i);

    return *this;
  }

  Bitset& Bitset::operator|=(const Bitset& that)
  {
    for (size_t i = 0; i < bits.size(); i++)
      bits.at(i) |= that.bits.at(i);

    return *this;
  }

  bool Bitset::operator==(const Bitset& that) const
  {
    if (bits.size() != that.bits.size())
      return false;

    for (size_t i = 0; i < bits.size(); i++)
    {
      if (bits.at(i) != that.bits.at(i))
        return false;
    }

    return true;
  }

  bool Bitset::operator!=(const Bitset& that) const
  {
    return !(*this == that);
  }
}
