cell
{
  f: i32;

  create(f: i32 = i32 0): cell
  {
    new {f = f}
  }
}

main(): i32
{
  let a = cell;
  var b = cell (i32(1 + 2));
  b = cell (i32(3 + 4));
  a.f = i32 3;
  a.f + b.f
}
