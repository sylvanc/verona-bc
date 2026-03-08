#pragma once

namespace vbci
{
  struct Region;
  struct Header;

  // Freeze a region: calculate SCCs, set up union-find for RC tracking,
  // and make all reachable objects immutable. Unreachable objects are
  // finalized and freed. The region is deleted.
  void freeze(Region* region, Header* root);
}
