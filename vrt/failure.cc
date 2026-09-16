#include "failure.h"

#include <exception>

namespace vrt
{
  [[noreturn]] void fail(Failure)
  {
    std::terminate();
  }
}
