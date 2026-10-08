#pragma once

namespace vbci
{
  struct Frame;
  struct Register;
  struct ValueBorrow;

  Register copy_graph(Frame& frame, ValueBorrow root);
}
