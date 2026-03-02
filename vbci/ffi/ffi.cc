#include "../platform.h"

#include "../array.h"
#include "../program.h"
#include "../value.h"

using namespace vbci;

VBCI_FFI bool platform_is_mac()
{
  return platform::is_mac();
}

VBCI_FFI bool platform_is_linux()
{
  return platform::is_linux();
}

VBCI_FFI bool platform_is_windows()
{
  return platform::is_windows();
}

VBCI_FFI Array* getargv()
{
  return Program::get().get_argv();
}

VBCI_FFI void printval(Value& val)
{
  std::cout << val.to_string() << std::endl;
}

VBCI_FFI uint64_t call_fn_ptr_u64(void (*fn)(uint64_t), uint64_t arg)
{
  fn(arg);
  return 0;
}

VBCI_FFI uint64_t call_fn_ptr_ret_u64(uint64_t (*fn)(uint64_t), uint64_t arg)
{
  return fn(arg);
}
