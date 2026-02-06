cown[T]
{
  create(): none
  {
    // TODO: throw
    none
  }

  read(self: cown[T]): cown[T]
  {
    :::read(self)
  }
}
