#pragma once

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vbc/format.h>
#include <vector>

namespace virc::vbc_backend
{
  using namespace ::vbc;

  template<typename T>
  struct sleb
  {
    T value;
    sleb(T value) : value(value) {}
  };

  template<typename T>
  struct uleb
  {
    T value;
    uleb(T value) : value(value) {}
  };

  template<typename T>
  struct d
  {
    DIOp op;
    T value;
    d(DIOp op, T value) : op(op), value(value) {}
  };

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, sleb<T>&& s)
  {
    static_assert(std::is_signed_v<T>);

    using U = std::make_unsigned_t<T>;
    auto bits = static_cast<U>(s.value);
    auto sign_mask = U{} - static_cast<U>(s.value < 0);
    auto value = (bits << 1) ^ sign_mask;
    return b << uleb(value);
  }

  template<>
  inline std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, sleb<float>&& s)
  {
    auto value = std::bit_cast<int32_t>(s.value);
    return b << sleb(value);
  }

  template<>
  inline std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, sleb<double>&& s)
  {
    auto value = std::bit_cast<int64_t>(s.value);
    return b << sleb(value);
  }

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, uleb<T>&& u)
  {
    auto value = u.value;

    while (value > 0x7F)
    {
      b.push_back((value & 0x7F) | 0x80);
      value >>= 7;
    }

    b.push_back(value);
    return b;
  }

  template<typename T>
  std::vector<uint8_t>& operator<<(std::vector<uint8_t>& b, d<T>&& d)
  {
    auto value = (d.value << 2) | +d.op;
    return b << uleb(value);
  }

  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, const std::string& str);
  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, const std::string_view& str);
}
