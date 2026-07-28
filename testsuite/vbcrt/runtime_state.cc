#include "vbcrt.h"

int main()
{
  vbcrt::reset_exit_code();
  if (vbcrt::get_exit_code() != 0)
    return 1;

  vbcrt::set_exit_code(7);
  if (vbcrt::get_exit_code() != 7)
    return 2;

  vbcrt::set_exit_code(3);
  if (vbcrt::get_exit_code() != 3)
    return 3;

  return 0;
}
