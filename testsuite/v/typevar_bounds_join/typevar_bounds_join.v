// When multiple concrete types flow into the same TypeVar, the
// solver joins them via LUB (covariant default per plan tiebreak).
// `to_union` accepts a value of any type by wrapping it in a Union
// with `none`; calling it with i32 vs i64 should reify two
// distinct copies, each with U bound to the concrete arg type.

wrap[U](v: U): U | none
{
  v
}

main(): none
{
  // Two reifications of `wrap`: one bound to i32, one to i64.
  match typevar_bounds_join::wrap[i32](5)
  {
    (n: i32) -> {
      match typevar_bounds_join::wrap[i64](7)
      {
        (m: i64) -> ffi::exit_code(n + m.i32)
      }
      else { ffi::exit_code 98 }
    }
  }
  else { ffi::exit_code 99 }
}
