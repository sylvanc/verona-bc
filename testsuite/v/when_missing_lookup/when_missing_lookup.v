foo
{
  once create(): cown[foo]
  {
    cown(new {})
  }

  bad(self: foo): none
  {
    when (foo, self._c) (f, c) -> {}
  }
}

main(): i32
{
  0
}
