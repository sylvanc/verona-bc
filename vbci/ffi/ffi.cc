#include "../array.h"
#include "../platform.h"
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
  auto s = val.to_string() + "\n";
  std::cout << s;
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

VBCI_FFI bool write_all(int fd, const uint8_t* data, size_t size)
{
  size_t total_written = 0;

  while (total_written < size)
  {
    ssize_t written = write(fd, data + total_written, size - total_written);

    if (written < 0)
    {
      // Retry on interrupt.
      if (errno == EINTR)
        continue;

      return false;
    }

    total_written += written;
  }

  return true;
}
