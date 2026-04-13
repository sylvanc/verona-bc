counter
{
  count: usize;

  create(): counter
  {
    new {count = 0}
  }
}

main(): none
{
  let c = counter;
}
