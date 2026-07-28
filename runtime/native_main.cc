#include "vrt.h"

#include <vrt/abi.h>

// defines main
int main()
{
  vrt::reset_exit_code();
  verona_main();
  return vrt::get_exit_code();
}
