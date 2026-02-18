counter
{
  count: usize;

  create(): counter
  {
    new {count = 0}
  }
}

main(): i32
{
  let c = counter;
  0
}
