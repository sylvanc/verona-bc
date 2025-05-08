#include "array.h"

#ifdef _WIN32
#  define VBCI_FFI  extern "C" __declspec(dllexport)
#else
#  define VBCI_FFI extern "C" [[gnu::used]] [[gnu::retain]]
#endif

VBCI_FFI vbci::Array* getargv()
{
  return vbci::Program::get().get_argv();
}

VBCI_FFI void printval(vbci::Value& val)
{
  std::cout << val.to_string() << std::endl;
}
