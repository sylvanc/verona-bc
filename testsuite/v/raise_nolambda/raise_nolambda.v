// Test that raise outside a lambda produces an error.
main(): none
{
  ffi::exit_code(raise 42)
}
