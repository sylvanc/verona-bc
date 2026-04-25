callback[T]
{
  _callable: T | none;
  _cb: ptr;

  create(): callback[T]
  {
    new { _callable = none, _cb = ptr }
  }

  create(callable: T): callback[T]
  {
    new { _callable = callable, _cb = :::make_callback(callable) }
  }

  bind(self: callback[T], callable: T): callback[T]
  {
    if self._cb != ptr { :::free_callback(self._cb) }
    self._callable = callable;
    self._cb = :::make_callback(callable);
    self
  }

  raw(self: callback[T]): ptr
  {
    :::codeptr_callback(self._cb)
  }

  final(self: callback[T]): none
  {
    if self._cb != ptr { :::free_callback(self._cb) }
  }
}
