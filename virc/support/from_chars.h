#pragma once

#include <charconv>

#if defined(PLATFORM_IS_MACOSX)

// Apple Clang provides <charconv> with from_chars_result and integer
// from_chars, but explicitly deletes the floating-point overloads.
// Provide custom floating-point from_chars using the native result type.
#  include <string>
#  include <system_error>

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
