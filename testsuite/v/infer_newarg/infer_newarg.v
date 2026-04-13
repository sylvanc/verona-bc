cell
{
  f: i32;

  create(f: i32): cell
  {
    new {f}
  }
}

main(): none
{
  let c = cell(1);
  ffi::exit_code(c.f)
}
