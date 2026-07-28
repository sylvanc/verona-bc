#include <vbcrt/abi.h>

static void (*const set_exit_code_signature)(int32_t) = set_exit_code;
static void (*const verona_main_signature)(void) = verona_main;

void verona_main(void)
{
  set_exit_code_signature(0);
  (void)verona_main_signature;
}
