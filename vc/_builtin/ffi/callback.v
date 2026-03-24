callback[T]
{
  _callable: T;
  _cb: ptr;

  create(callable: T): callback[T]
  {
    new { _callable = callable, _cb = :::make_callback(callable) }
  }

  raw(self: callback[T]): ptr
  {
    :::codeptr_callback(self._cb)
  }

  final(self: callback[T]): none
  {
    :::free_callback(self._cb)
  }
}
