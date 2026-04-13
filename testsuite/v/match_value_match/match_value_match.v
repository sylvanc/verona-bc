// Test that match with value and type arms infers literal types correctly.
// The value match arm `(7) -> 100` returns a default int literal, which
// is inferred as i32 from the `let a: i32 = ...` context. The type match
// arm `(x: i32) -> x` also returns i32. Both arms agree, so the match
// result is i32 and `a == 100` succeeds.
main(): none
{
  var result = i32 0;
  let a: i32 = (match 7 { (7) -> 100; (x: i32) -> x; }) else (0);
  if !(a == 100) { result = result + 1 }
  ffi::exit_code result
}
