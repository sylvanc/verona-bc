#pragma once

#include <verona.h>

namespace vbci
{
  void run_loop();
  void stop_loop();

  void add_external(verona::rt::Work* work);
  void remove_external(verona::rt::Work* work);
}
