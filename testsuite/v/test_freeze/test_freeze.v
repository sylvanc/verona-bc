leaf
{
  val: i32;

  create(val: i32): leaf
  {
    new {val}
  }
}

main(): i32
{
  var result = 0;

  // Test simple freeze.
  var a = leaf(10);
  freeze(a);
  if a.val != 10 { result = result + 1; }

  // Test double freeze (no-op).
  var b = leaf(55);
  freeze(b);
  freeze(b);
  if b.val != 55 { result = result + 2; }

  result
}
