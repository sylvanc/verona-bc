// Test: once function compiles and can be called multiple times.
// Returns same value (42) each time.

once answer(): i32
{
  i32 42
}

main(): none
{
  let a = memo_basic::answer();
  let b = memo_basic::answer();
  var result = i32 0;
  if a != i32 42 { result = result + i32 1 }
  if b != i32 42 { result = result + i32 2 }
  if a != b { result = result + i32 4 }
  ffi::exit_code result
}
