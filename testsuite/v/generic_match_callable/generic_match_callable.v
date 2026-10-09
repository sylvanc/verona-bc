// Verify that direct and nested generic match binders retain their narrowed
// callable type through reification.

call[T](value: T | none, x: u64): u64
{
  (match value
  {
    (f: T) -> f(x);
  }) else (0)
}

nested[T](outer: T | none, inner: T | none, x: u64): u64
{
  (match outer
  {
    (f: T) ->
      (match inner
      {
        (g: T) -> f(g(x));
      }) else (0);
  }) else (0)
}

main(): none
{
  var result = 0;
  let f = (x: u64): u64 -> x + 10;

  if generic_match_callable::call(f, 32) != 42
  {
    result = result + 1
  }

  if generic_match_callable::nested(f, f, 22) != 42
  {
    result = result + 2
  }

  ffi::exit_code result
}
