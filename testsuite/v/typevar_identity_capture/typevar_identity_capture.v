// Capture identity: var-captures (mutated through ref[T]) and
// let-captures (read-only) both preserve the enclosing function's
// TypeVar Locations. The lambda's apply param `x: U` and the
// captured var `result: U | none` share the same alpha_U identity
// so the constraint solver binds U from the concrete element type
// passed to `f`.

last_via_capture[T, K, U](c: T): U | none
{
  var result: U | none = none;
  c.each (k: K, x: U): none -> {
    result = x
  }
  result
}

main(): none
{
  let a = array[i32]::fill(2);
  a(0) = 7;
  a(1) = 11;
  match typevar_identity_capture::last_via_capture(a)
  {
    (n: i32) -> ffi::exit_code n
  }
  else { ffi::exit_code 99 }
}
