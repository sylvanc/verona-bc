// Test that mismatched types in match arms produce a compile-time error.
// The value match arm `(7) -> 100` returns u64 (unrefined bare literal),
// while the type match arm `(x: i32) -> x` returns i32.
// The result type `Union(u64, i32)` should fail the typecheck at the usage
// site (a == 100), rather than producing a runtime "bad type" error.
main(): i32
{
  var result = i32 0;
  let a: i32 = (match 7 { (7) -> 100; (x: i32) -> x; }) else (0);
  if !(a == 100) { result = result + 1 }
  result
}
