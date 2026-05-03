// Regression test: find_method_return_type used to use range-for over
// map_order, but its inner call to find_func_return_type can recursively
// push_back to map_order via reify_emitted_type -> find_or_push,
// invalidating the range-for iterator. The next iteration would read
// freed memory (presents as a null Node entry), then crash on operator/.
//
// The trigger is: a generic class with `each(f: T -> none)` instantiated
// with at least two distinct Ts where the second T's instantiation does
// extra reification work (e.g., string with its many constructors).
//
// Before the fix, this program segfaulted in vc::Reifier::find_method_return_type.

list[T]
{
  _data: array[T];
  _len: usize;
  create(): list[T] { new { _data = array[T]::alloc(4), _len = 0 } }
  push(self: list[T], v: T): list[T]
  {
    self._data()(self._len) = v;
    self._len = self._len + 1;
    self
  }
  each(self: list[T], f: T -> none): none
  {
    var i: usize = 0;
    while i < self._len { f(self._data()(i)); i = i + 1 }
  }
}

main(): none
{
  let l = list[i32]();
  l.push 1; l.push 2; l.push 3;
  var sum = i32 0;
  l.each (x: i32) -> { sum = sum + x }

  // Reifying list[string] triggers extra work in find_or_push that
  // grows map_order while find_method_return_type is iterating it.
  let s = list[string]();
  s.push "alpha"
}
