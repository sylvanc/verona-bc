#include "vrt.h"

#include <cstdio>
#include <cstdlib>
#include <vrt/error.h>
#include <vrt/program.h>
#include <vrt/thread.h>

namespace
{
  void run_program(void*)
  {
    verona_program_entry();
  }
}

// native main() in libvrt for runtime initialization and teardown.
/*
 * native_main.cc::main() is the executable entry point and performs runtime
 * initialization and teardown. verona_program_entry is the generated, exported
 * C-ABI trampoline using the C calling convention. verona_fn_main contains the
 * generated implementation of Verona @main and uses LLVM tailcc.
 */
int main()
{
  // This single-threaded bootstrap keeps runtime lifecycle out of generated
  // code. Scheduler startup will later bind each worker before dispatching
  // verona_program_entry as the initial job.
  vrt_runtime_init();
  vrt_program_init(&verona_program);
  vrt_thread_init();
  vrt_invocation_begin();
  vrt_error_info error{};
  if (!vrt_try_invoke(run_program, nullptr, &error))
  {
    std::fprintf(
      stderr, "runtime error: %s\n", vrt_error_message(error.code));
    vrt_thread_deinit();
    return EXIT_FAILURE;
  }

  auto exit_code = vrt::get_exit_code();
  vrt_thread_deinit();
  return exit_code;
}
