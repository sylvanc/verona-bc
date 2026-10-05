#include "encoder.h"

namespace virc::vbc_backend
{
  ByteBuffer& operator<<(ByteBuffer& buffer, const std::string& string)
  {
    buffer << uleb(string.size());
    buffer.insert(buffer.end(), string.begin(), string.end());
    return buffer;
  }

  ByteBuffer& operator<<(ByteBuffer& buffer, std::string_view string)
  {
    buffer << uleb(string.size());
    buffer.insert(buffer.end(), string.begin(), string.end());
    return buffer;
  }
}