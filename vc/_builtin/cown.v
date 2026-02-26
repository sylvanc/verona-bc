cown[T]
{
  create(): none
  {
    // TODO: error
    none
  }

  read(self: cown[T]): cown[T]
  {
    :::read(self)
  }
}
