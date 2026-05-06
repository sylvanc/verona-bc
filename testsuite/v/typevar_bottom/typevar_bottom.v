// A polymorphic function with zero T-evidence in its body.
// solve(α_T) returns ⊥ (empty Union), and the function reifies
// successfully — the returned `none` is sound regardless of T.

empty_for_t[T](): T | none
{
  none
}

main(): none
{
  match typevar_bottom::empty_for_t[i32]()
  {
    (n: i32) -> ffi::exit_code 1
  }
  else { ffi::exit_code 0 }
}
