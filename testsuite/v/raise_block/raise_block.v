// Test non-local return via raise in a lambda (block).
// A lambda containing raise captures the raise target at creation time.
// When the lambda is called, it restores the raise target and raises,
// returning directly to the creating function's caller.

find_first(a: i32, b: i32, target: i32): i32
{
  // This lambda uses raise to return directly from find_first.
  let check = (x: i32) -> {
    if x == target
    {
      raise x
    }
  }
  check(a);
  check(b);
  // If neither matched, return 0.
  0
}

main(): i32
{
  let result = raise_block::find_first(10, 42, 42);
  result
}
