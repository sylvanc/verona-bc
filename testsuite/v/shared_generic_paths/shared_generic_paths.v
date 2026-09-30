// Tests: generated calls inside a deeply nested generic class retain the
// symbolic type arguments contributed by every enclosing class.
// Failure mode: `caller` reifies `target` with a missing or incorrect outer
// binder, causing compilation failure or a wrong returned value.
// Assumptions: both explicit instantiations share the same generated call path
// but bind `A` independently; `B` and `C` must not replace it.

outer[A]
{
  middle[B]
  {
    inner[C]
    {
      target(a: A, x: B): A
      {
        a
      }

      caller(a: A, x: B): A
      {
        inner[C]::target(a, x)
      }
    }
  }
}

main(): none
{
  let first =
    outer[u64]::middle[i32]::inner[bool]::caller(u64 42, 1);
  let second =
    outer[i32]::middle[i32]::inner[bool]::caller(i32 24, 2);

  if first != 42 { ffi::exit_code 1 }
  if second != 24 { ffi::exit_code 2 }
}
