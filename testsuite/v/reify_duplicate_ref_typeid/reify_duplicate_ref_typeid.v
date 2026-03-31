maker
{
  create(): maker
  {
    new {}
  }

  one(self: maker, handler: ()->none): ()->none
  {
    () -> handler()
  }

  two(self: maker, handler: ()->none): ()->none
  {
    () -> handler()
  }
}

main(): i32
{
  let h = () -> {}
  let m = maker;
  let f1 = m.one(h);
  let f2 = m.two(h);
  f1();
  f2();
  0
}
