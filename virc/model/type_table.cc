#include "type_table.h"

namespace virc
{
  size_t VecHash::operator()(const std::vector<uint8_t>& v) const noexcept
  {
    auto h = size_t(14695981039346656037ull);

    for (auto b : v)
      h = (h ^ b) * size_t(1099511628211ull);

    return h;
  }
}
