// Regression test: merge_refined_type used to add nested Union nodes
// as single elements without flattening. When refine_function_params
// re-iterated the reify fixpoint, an `actual` whose type was already a
// Union (typically from a multi-return-path body with `return none` plus
// a non-none final expression) was added wholesale to `merged`. The
// next round saw a nested-Union in `current` that didn't structurally
// equal the new wholesale Union from `actual`, so it added another
// nested copy. The merged size grew geometrically (~3x per round) and
// vc spun until OOM in NodeDef::clone over the ever-growing type.
//
// The repro is a generic class method whose body has both `return none`
// and a final non-none expression (so the body's static type is a
// Union), AND a call site in a match where the match arm gives a
// concrete expected type. Without the fix vc spins; with the fix
// inference cleanly errors that U cannot be resolved without an
// explicit type argument, and explicit type args succeed.

helper[T]
{
  thing(self: helper[T], c: array[T]): T | none
  {
    if c.size == usize 0 { return none }
    c(usize 0)
  }
}

main(): none
{
  let h = helper[i32];
  let a = array[i32]::fill(3, i32 7);

  match h.thing(a)
  {
    (m: i32) -> none
  }
  else { none }
}
