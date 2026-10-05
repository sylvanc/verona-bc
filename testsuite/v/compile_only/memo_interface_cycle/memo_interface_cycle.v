// Once-cycle detection must include calls resolved through shape dispatch.
// first reaches second through provider.get, while second calls first directly.
// The compiler must reject the cycle; missing the dynamic edge would compile it.

shape provider
{
  get(self: self): i32;
}

implementation
{
  get(self: implementation): i32
  {
    memo_interface_cycle::second()
  }
}

invoke(value: provider): i32
{
  value.get
}

once first(): i32
{
  memo_interface_cycle::invoke(implementation)
}

once second(): i32
{
  memo_interface_cycle::first()
}

main(): none
{
  ffi::exit_code(memo_interface_cycle::first())
}
