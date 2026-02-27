cown[T]
{
  create(): none
  {
    none
  }

  read(self: cown[T]): cown[T]
  {
    :::read(self)
  }
}
