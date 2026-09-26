#include "encoder.h"

namespace virc::vbc_backend
{
  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, const std::string& str)
  {
    b << uleb(str.size());
    b.insert(b.end(), str.begin(), str.end());
    return b;
  }

  std::vector<uint8_t>&
  operator<<(std::vector<uint8_t>& b, const std::string_view& str)
  {
    b << uleb(str.size());
    b.insert(b.end(), str.begin(), str.end());
    return b;
  }
}
