#include <vrt/function.h>

namespace
{
  void first_entry() {}
  void second_entry() {}
}

int main()
{
  const vrt::Function first{1, "first", &first_entry};
  const vrt::Function second{2, "second", &second_entry};

  if (
    (vrt_func_entry(&first) != &first_entry) ||
    (vrt_func_entry(&second) != &second_entry))
    return 1;

  return 0;
}