// Tests: a sibling module may use a definition imported by a preceding `use`
// declaration, and the resulting unqualified call remains reachable.
// Failure mode: `consumer::get` cannot resolve `value`, resolves its own import
// recursively, or returns the wrong sibling definition.
// Assumptions: module imports are processed in source order and `consumer`
// imports `provider` before importing `list` through it.

main(): none
{
  var result = 0;

  if consumer::get() != 42
  {
    result = result + 1
  }

  ffi::exit_code result
}
