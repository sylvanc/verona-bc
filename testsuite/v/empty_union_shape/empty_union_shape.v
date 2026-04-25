// Regression: methods whose parameter shapes have no implementor must
// not be reified just because the class is reachable. Before the fix,
// merely constructing `c` would cause `c::go` to be reified (via
// "all_receivers" fallback on any `.go` CallDyn elsewhere in the
// program), pulling `never_impl` into reification as an empty union
// and crashing program load in layout_union_type.
//
// With the reify fix, `c::go` is not reified (nothing dispatches `.go`
// on c), so no empty union is materialized and the program runs clean.

shape never_impl
{
  apply(self: self, x: i32): none;
}

c
{
  create(): c { new {} }

  go(self: c, h: never_impl): none
  {
    h(42)
  }
}

main(): none
{
  c();
  none
}
