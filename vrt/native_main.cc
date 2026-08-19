#include "vrt.h"

#include <vrt/program.h>

// native main() in libvrt for runtime initialization and teardown.
int main()
{
  // This single-threaded bootstrap keeps runtime lifecycle out of generated
  // code. Scheduler startup will later bind each worker before dispatching
  // verona_program_entry as the initial job.
  vrt::init_thread();
  vrt::reset_exit_code();
  verona_program_entry();
  auto exit_code = vrt::get_exit_code();
  vrt::deinit_thread();
  return exit_code;
}
