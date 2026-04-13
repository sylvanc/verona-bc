main(): none
{
  ffi::exit_code(if dash_dir_modules::root_value() == 41 { 0 } else { 1 })
}

root_value(): i32
{
  41
}
