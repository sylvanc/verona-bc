#pragma once

#include "lang.h"

namespace vbcc
{
  using namespace trieste::wf::ops;

  // clang-format off
  inline const auto wfParser =
      (Top <<= (Directory | File)++)
    | (Directory <<= (Directory | File)++)
    // | (File <<= (Group | List | Equals)++)
    // | (Brace <<= (Group | List | Equals)++)
    // | (Paren <<= (Group | List | Equals)++)
    // | (Square <<= (Group | List | Equals)++)
    // | (List <<= (Group | Equals)++)
    // | (Equals <<= Group++)
    // | (Group <<= wfParserTokens++)
    ;
  // clang-format on
}
