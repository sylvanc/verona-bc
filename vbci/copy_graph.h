#pragma once

namespace vbci
{
  struct Frame;
  struct Register;

  /**
   * Copy a graph and consume the source reference without finalizing objects
   * whose semantic lifetimes continue in the copy.
   */
  Register copy_graph_and_consume(Frame& frame, Register root);
}
