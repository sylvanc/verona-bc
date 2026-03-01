callback
{
  create[T](callable: T): callback { :::make_callback(callable) }
  ptr(self: callback): ptr { :::callback_ptr(self) }
  free(self: callback): none { :::free_callback(self) }
}
