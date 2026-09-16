#include "failure.h"

#include <vrt/function.h>

extern "C" VRT_EXPORT vrt_func_ptr vrt_func_get_ptr(const vrt_func* func)
{
  if ((func == nullptr) || (func->entry == nullptr))
    vrt::fail(vrt::Failure::invalid_function_state);

  return func->entry;
}
