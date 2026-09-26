#pragma once

#include "../model/name_table.h"
#include "encoder.h"

namespace virc::vbc_backend
{
  void encode_string_table(ByteBuffer& output, ST& table);
}