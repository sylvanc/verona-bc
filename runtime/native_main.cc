#include "vbcrt.h"

#include <vbcrt/abi.h>

// defines main
int main(){
  vbcrt::reset_exit_code();
  verona_main();
  return vbcrt::get_exit_code();
}
