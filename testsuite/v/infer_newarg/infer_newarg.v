cell
{
  f: i32;

  create(f: i32): cell
  {
    new {f = f}
  }
}

main(): i32
{
  let c = cell(1);
  c.f
}
