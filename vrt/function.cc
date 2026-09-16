#include <vrt/function.h>

#include <exception>

extern "C" VRT_EXPORT vrt_func_ptr vrt_func_get_ptr(const vrt_func* func)
{
  if ((func == nullptr) || (func->entry == nullptr))
    std::terminate();

  return func->entry;
}
