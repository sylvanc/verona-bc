#include "vrt.h"

int main()
{
  vrt::reset_exit_code();
  if (vrt::get_exit_code() != 0)
    return 1;

  vrt::set_exit_code(7);
  if (vrt::get_exit_code() != 7)
    return 2;

  vrt::set_exit_code(3);
  if (vrt::get_exit_code() != 3)
    return 3;

  return 0;
}
