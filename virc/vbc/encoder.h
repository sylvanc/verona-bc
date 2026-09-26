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
  using ByteBuffer = std::vector<uint8_t>;

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
    ::vbc::DIOp op;
    T value;
    d(::vbc::DIOp op, T value) : op(op), value(value) {}
  };

  template<typename T>
  ByteBuffer& operator<<(ByteBuffer& buffer, uleb<T>&& encoded)
  {
    auto value = encoded.value;

    while (value > 0x7F)
    {
      buffer.push_back((value & 0x7F) | 0x80);
      value >>= 7;
    }

    buffer.push_back(value);
    return buffer;
  }

  template<typename T>
  ByteBuffer& operator<<(ByteBuffer& buffer, sleb<T>&& encoded)
  {
    static_assert(std::is_signed_v<T>);

    using U = std::make_unsigned_t<T>;
    auto bits = static_cast<U>(encoded.value);
    auto sign_mask = U{} - static_cast<U>(encoded.value < 0);
    auto value = (bits << 1) ^ sign_mask;
    return buffer << uleb(value);
  }

  template<>
  inline ByteBuffer& operator<<(ByteBuffer& buffer, sleb<float>&& encoded)
  {
    auto value = std::bit_cast<int32_t>(encoded.value);
    return buffer << sleb(value);
  }

  template<>
  inline ByteBuffer& operator<<(ByteBuffer& buffer, sleb<double>&& encoded)
  {
    auto value = std::bit_cast<int64_t>(encoded.value);
    return buffer << sleb(value);
  }

  template<typename T>
  ByteBuffer& operator<<(ByteBuffer& buffer, d<T>&& encoded)
  {
    auto value = (encoded.value << 2) | +encoded.op;
    return buffer << uleb(value);
  }

  ByteBuffer& operator<<(ByteBuffer& buffer, const std::string& string);
  ByteBuffer& operator<<(ByteBuffer& buffer, std::string_view string);
}