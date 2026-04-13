main(): none
{
  ffi::exit_code(
    if false
    {
      i32 0
    }
    else if false
    {
      i32 1
    }
    else (i32 2)
  )
}
