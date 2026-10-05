// Once ordering must include dependencies reached through shape dispatch.
// a_dependent invokes implementation.get, which loads z_dependency and returns
// 42. Missing that dynamic edge initializes in the wrong order and fails or
// returns a nonzero exit code. Expected exit code: 0.

shape provider
{
  get(self: self): i32;
}

implementation
{
  get(self: implementation): i32
  {
    memo_interface_deps::z_dependency()
  }
}

invoke(value: provider): i32
{
  value.get
}

once a_dependent(): i32
{
  memo_interface_deps::invoke(implementation)
}

once z_dependency(): i32
{
  42
}

main(): none
{
  let result = memo_interface_deps::a_dependent();
  ffi::exit_code(if result == 42 { 0 } else { 1 })
}
