holder
{
  cb: ffi::callback[() -> none];

  create(): holder
  {
    let cb = ffi::callback (): none -> {}
    new {cb}
  }
}

main(): none
{
  let h = holder();
}
