// Regression test: generic union field types must not be narrowed by
// refine_function_params. When _node[T]::create is first called with
// next=none, the next parameter (type _node[T]|none) must keep its union
// type for subsequent calls that pass a _node[T] value.

_node[T]
{
  val: T;
  next: _node[T] | none;

  create(val: T, next: _node[T] | none): _node[T]
  {
    new { val, next }
  }
}

main(): none
{
  // First call: next=none — must not narrow create's param to none
  let n1 = _node[i32](1, none);
  // Second call: next=n1 — would fail typecheck if param narrowed to none
  let n2 = _node[i32](2, n1);
  // Call inside match arm with matched value
  match n2.next
  {
    (old: _node[i32]) -> _node[i32](3, old);
  }
  ffi::exit_code 0
}
