#include "vrt.h"

#include <vrt/program.h>
#include <vrt/thread.h>

namespace
{
  const vrt::Program program{0, nullptr, 0, nullptr};
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt_thread_init();

  set_exit_code(7);
  vrt_invocation_begin();
  if (vrt::get_exit_code() != 0)
    return 1;

  set_exit_code(9);
  vrt_invocation_begin();
  if (vrt::get_exit_code() != 0)
    return 2;

  vrt_thread_deinit();
  return 0;
}
