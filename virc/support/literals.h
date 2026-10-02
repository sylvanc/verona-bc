#pragma once

#include "../model/type_table.h"

#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vir.h>

#if defined(PLATFORM_IS_MACOSX)

namespace std
{
  template<typename T>
  std::enable_if_t<std::is_floating_point_v<T>, from_chars_result>
  from_chars(const char* first, const char* last, T& value)
  {
    std::string str(first, last);
    try
    {
      if constexpr (std::is_same_v<T, float>)
        value = std::stof(str);
      else if constexpr (std::is_same_v<T, double>)
        value = std::stod(str);
      else if constexpr (std::is_same_v<T, long double>)
        value = std::stold(str);
      return {last, {}};
    }
    catch (const std::invalid_argument&)
    {
      return {first, std::errc::invalid_argument};
    }
    catch (const std::out_of_range&)
    {
      return {first, std::errc::result_out_of_range};
    }
  }
}

#endif

namespace virc
{
  using namespace trieste;
  using namespace vir;

  PrimitiveKind val(Node ptype);
  Node ffi_struct_result_type();
  std::string unescape(const std::string_view& in);

  template<typename T>
  std::from_chars_result
  from_chars_sep(const Node& node, const char* s, size_t n, T& value)
  {
    auto end = s + n;

    if constexpr (std::is_integral_v<T>)
    {
      if (node == Bin)
        return std::from_chars(s + 2, end, value, 2);
      if (node == Oct)
        return std::from_chars(s + 2, end, value, 8);
      if (node == Hex)
        return std::from_chars(s + 2, end, value, 16);
      if (node == Int)
        return std::from_chars(s, end, value, 10);
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
      if (node->in({Float, HexFloat}))
        return std::from_chars(s, end, value);
    }

    return {s, std::errc::invalid_argument};
  }

  template<typename T, char Sep = '_'>
  std::from_chars_result from_chars_sep(const Node& node, T& value)
  {
    auto text = node->location().view();

    if (node == Char)
    {
      auto unescaped = unescape(text);

      if (unescaped.starts_with("error:"))
        return {text.data(), std::errc::invalid_argument};

      if (unescaped.empty() || (unescaped.size() > sizeof(uint64_t)))
        return {text.data(), std::errc::result_out_of_range};

      uint64_t packed = 0;

      for (auto character : unescaped)
        packed = (packed << 8) | static_cast<uint8_t>(character);

      if constexpr (std::is_signed_v<T>)
      {
        if (packed > static_cast<uint64_t>(std::numeric_limits<T>::max()))
          return {text.data(), std::errc::result_out_of_range};
      }
      else
      {
        if (packed > std::numeric_limits<T>::max())
          return {text.data(), std::errc::result_out_of_range};
      }

      value = static_cast<T>(packed);
      return {text.data() + text.size(), {}};
    }

    if (text.find(Sep) == std::string_view::npos)
      return from_chars_sep<T>(node, text.data(), text.size(), value);

    std::string stripped;
    stripped.reserve(text.size());

    for (char character : text)
    {
      if (character != '_')
        stripped.push_back(character);
    }

    return from_chars_sep<T>(node, stripped.data(), stripped.size(), value);
  }

  template<typename T, char Sep = '_'>
  T from_chars_sep_v(const Node& node)
  {
    T value = 0;
    from_chars_sep<T, Sep>(node, value);
    return value;
  }
}