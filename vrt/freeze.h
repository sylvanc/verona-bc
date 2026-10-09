#pragma once

namespace vrt
{
  struct Header;

  /** Freeze a reachable RC-region graph; return false for unsupported roots. */
  bool freeze(Header* root);
}