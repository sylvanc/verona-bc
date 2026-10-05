#include "sanitizer.h"

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define VBCI_ADDRESS_SANITIZER
#  endif
#endif

#if defined(__SANITIZE_ADDRESS__) && !defined(VBCI_ADDRESS_SANITIZER)
#  define VBCI_ADDRESS_SANITIZER
#endif

#if defined(VBCI_ADDRESS_SANITIZER)
#  include <sanitizer/lsan_interface.h>
#endif

namespace vbci
{
  void ignore_process_lifetime_allocation(const void* allocation)
  {
#if defined(VBCI_ADDRESS_SANITIZER)
    __lsan_ignore_object(allocation);
#else
    (void)allocation;
#endif
  }
}
