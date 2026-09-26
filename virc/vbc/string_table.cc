#include "string_table.h"

namespace virc::vbc_backend
{
  void encode_string_table(ByteBuffer& output, ST& table)
  {
    output << uleb(table.size());

    for (size_t index = 0; index < table.size(); index++)
      output << table.at(index);
  }
}