#pragma once

namespace vbci
{
  struct Header;

  // Freeze the reachable subgraph from root: calculate SCCs, set up
  // union-find for ARC tracking, and make reachable objects immutable.
  // The region survives with unfrozen objects; stack_rc is adjusted.
  // Sub-regions reachable from frozen objects have ownership cleared.
  // No-op (returns true) if root is already immutable or immortal.
  // Returns false if root is on the stack or in a frame-local region.
  bool freeze(Header* root);

  // Freeze a frame-local object graph in place. Calculates SCCs, marks
  // reachable frame-local objects as immutable, and removes them from their
  // source regions. Skips heap-region objects. No sweep or region deletion —
  // unreachable objects remain in the frame-local region for normal teardown.
  void freeze_local(Header* root);
}
