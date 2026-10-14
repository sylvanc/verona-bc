#include "failure.h"

#include <vrt/function.h>

extern "C" VRT_EXPORT vrt::FunctionEntry
vrt_func_entry(const vrt::Function* func)
{
  internal_check(
    (func != nullptr) && (func->entry != nullptr),
    vrt::Failure::invalid_function_state);

  return func->entry;
}
