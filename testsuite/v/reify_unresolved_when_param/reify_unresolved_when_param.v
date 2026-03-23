state
{
  value: i32;
}

holder[T]
{
  _c: cown[T];

  create(t: T): holder[T]
  {
    new {_c = cown t}
  }
}

runner
{
  run[A, B](some: A, handler: B->none): none
  {
    when some._c c ->
    {
      handler(*c)
    }
  }
}

main(): i32
{
  let h = holder(new {value = 42});
  runner::run(h, s -> none);
  0
}
