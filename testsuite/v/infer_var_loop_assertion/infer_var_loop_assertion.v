// Test: a var with a typed declaration must keep its declared type
// across multi-pass infer invocations, even after the TypeAssertion
// emitted by ANF is consumed.
//
// Regression: previously, `process_function` erased TypeAssertion
// nodes during the first invocation's const finalizer. The deferred
// re-processing (and the post-DefaultInt-sweep re-processing) ran
// without the assertion, so env[i] entered unfixed and the loop's
// backward flow could clobber it via Copy chains. The const literal
// `8` in `i < 8` ended up typed as i32 (instead of usize), driving
// the lookup of `<` to `i32::<` and producing typecheck errors at
// the call site for `v(i)` (which expects `i: usize`).
//
// Fix: TypeAssertion nodes survive the const finalizer; a final
// post-pass sweep removes them after all process_function calls
// have completed. TypeAssertions whose type contains a TypeVar
// (capture-ref placeholders inserted before captures are resolved)
// are still skipped at infer time so refinement can fill them in.

mybox
{
  scan(self: mybox, v: array[i32]): i32
  {
    var sum = i32 0;
    var i: usize = 0;
    while i < 8
    {
      let aa = v(i);
      let xx = i.i32;
      let yy = xx + 1;
      if aa == yy { sum = sum + 1 };
      i = i + 1
    };
    sum
  }

  create(): mybox { new {} }
}

main(): none
{
  let m = mybox();
  let arr = ::(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8);
  let result = m.scan arr;
  if result == 8 { ffi::exit_code 0 } else { ffi::exit_code 1 }
}
