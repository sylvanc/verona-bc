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
  }

  remove(self: external): none
  {
    when (self.c) (c) ->
    {
      :::remove_external
    }
  }
}
