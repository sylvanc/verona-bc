#pragma once

namespace vbci
{
  struct Header;

  // Freeze the reachable subgraph from root: calculate SCCs, set up
  // union-find for ARC tracking, and make reachable objects immutable.
  // The region survives with unfrozen objects; stack_rc is adjusted.
  // Sub-regions reachable from frozen objects are frozen in the same way.
  // No-op (returns true) if root is already immutable or immortal.
  // Returns false if root is on the stack.
  bool freeze(Header* root);
}
