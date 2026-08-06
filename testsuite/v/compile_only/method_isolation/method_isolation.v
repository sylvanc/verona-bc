// Regression: uncalled methods on unrelated classes should NOT be reified.
//
// Two classes both define a method named `go` with arity 2, but taking
// different shape-typed handlers. Only `a::go` is called in main. Before
// the reify fix, `b::go` would also be reified (because the CallDyn's
// receiver set defaulted to "all classes" when local_types lacked the
// receiver's type), which pulled `b`'s unused handler shape into
// reification. That shape had no concrete implementor → empty-union
// type → crash at program load via layout_union_type.
//
// With the fix, only `a::go` is reified, `b::go` and `b::handler_b` are
// not, and the program runs cleanly.

shape handler_a
{
  apply(self: self, x: i32): none;
}

shape handler_b
{
  apply(self: self, x: i32): none;
}

a
{
  create(): a { new {} }

  go(self: a, h: handler_a): none
  {
    h(42)
  }
}

b
{
  create(): b { new {} }

  go(self: b, h: handler_b): none
  {
    h(42)
  }
}

main(): none
{
  var result = i32 0;

  a().go x ->
  {
    if x == 42
    {
      result = result + 1
    }
    none
  };

  if result == 1
  {
    ffi::exit_code(i32 0)
  }
  else
  {
    ffi::exit_code(i32 1)
  }
}
