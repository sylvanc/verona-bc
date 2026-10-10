#pragma once

namespace vbci
{
  struct Cown;
  struct Register;

  /**
   * Copy a graph and consume the source reference without finalizing objects
   * whose semantic lifetimes continue in the copy.
   */
  Register copy_graph_and_consume(Register root);

  void copy_graph_and_consume(Cown& cown);
}
