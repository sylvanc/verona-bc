cell
{
  f: i32;

  create(f: i32 = i32 0): cell
  {
    new {f}
  }
}

main(): i32
{
  let a = cell;
  var b = cell (1 + 2);
  b = cell (3 + 4);
  a.f = 3;
  a.f + b.f
}
