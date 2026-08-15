#include "vrt.h"

#include <vrt/abi.h>

// defines main
int main()
{
  // This single-threaded bootstrap keeps runtime lifecycle out of generated
  // code. Scheduler startup will later bind each worker before dispatching
  // verona_main as the initial job.
  vrt::init_thread();
  vrt::reset_exit_code();
  verona_main();
  auto exit_code = vrt::get_exit_code();
  vrt::deinit_thread();
  return exit_code;
}
