register_external_notify[T](f: T): none
{
  :::register_external_notify(callback(f))
}

external
{
  c: cown[none];

  once create(): external
  {
    new {c = cown none}
  }

  add(self: external): none
  {
    when (self.c) (c) ->
    {
      :::add_external
    }

    none
  }

  remove(self: external): none
  {
    when (self.c) (c) ->
    {
      :::remove_external
    }

    none
  }
}
