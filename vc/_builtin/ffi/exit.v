use
{
  set_exit_code = "set_exit_code"(i32): none;
}

exit_code(code: i32): none
{
  :::set_exit_code(code)
}
