cell
{
  f: i32;

  create(f: i32 = 0): cell
  {
    new f
  }
}

main(): i32
{
  let a = cell;
  let b = cell (1 + 2);
  a.f + b.f
}
