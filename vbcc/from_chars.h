#pragma once

#include "../vbci/platform.h"

// Apple Clang does not include from_chars, so we provide our own
#if defined(PLATFORM_IS_MACOSX)

#  include <system_error>

namespace std
{
  struct from_chars_result
  {
    const char* ptr;
    std::errc ec;
  };

  template<typename T>
  from_chars_result from_chars(const char* first, const char* last, T& value)
  {
    std::string str(first, last);
    try
    {
      if constexpr (std::is_same<T, float>::value)
        value = std::stof(str);
      else if constexpr (std::is_same<T, double>::value)
        value = std::stod(str);
      else if constexpr (std::is_same<T, long double>::value)
        value = std::stold(str);
      else
        return {first, std::errc::invalid_argument};
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

  template<typename T>
  from_chars_result
  from_chars(const char* first, const char* last, T& value, int base)
  {
    std::string str(first, last);
    try
    {
      value = std::stol(str, nullptr, base);
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
