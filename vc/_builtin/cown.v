cown[T]
{
  create(some: T): cown[T]
  {
    when () () ->
    {
      some
    }
  }

  read(self: cown[T]): cown[T]
  {
    :::read(self)
  }
}
