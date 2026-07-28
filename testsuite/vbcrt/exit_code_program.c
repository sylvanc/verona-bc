#include <vbcrt/abi.h>

void verona_main(void)
{
#if defined(VBCRT_TEST_FIRST_EXIT_CODE)
  set_exit_code(VBCRT_TEST_FIRST_EXIT_CODE);
#endif

#if defined(VBCRT_TEST_LAST_EXIT_CODE)
  set_exit_code(VBCRT_TEST_LAST_EXIT_CODE);
#endif
}
