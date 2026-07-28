#include <vrt/abi.h>

void verona_main(void)
{
#if defined(VRT_TEST_FIRST_EXIT_CODE)
  set_exit_code(VRT_TEST_FIRST_EXIT_CODE);
#endif

#if defined(VRT_TEST_LAST_EXIT_CODE)
  set_exit_code(VRT_TEST_LAST_EXIT_CODE);
#endif
}
