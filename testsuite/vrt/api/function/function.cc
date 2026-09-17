#include <vrt/function.h>

namespace
{
  void first_entry() {}
  void second_entry() {}
}

int main()
{
  const vrt::Func first{1, "first", &first_entry};
  const vrt::Func second{2, "second", &second_entry};

  if (
    (vrt_func_get_ptr(&first) != &first_entry) ||
    (vrt_func_get_ptr(&second) != &second_entry))
    return 1;

  return 0;
}