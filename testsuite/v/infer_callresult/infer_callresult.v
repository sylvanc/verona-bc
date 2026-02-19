wrapper[T]
{
  val: T;

  create(val: T): wrapper[T]
  {
    new {val = val}
  }
}

maker
{
  value(a: i32): i32
  {
    a
  }
}

main(): i32
{
  // maker::value(3) returns i32, recorded in env via call result tracking.
  // wrapper(x): x contributes i32 (from call result) for T inference,
  // so T=i32. Without call result tracking, x has no env entry and
  // T would remain unresolved, causing a reify error.
  let x = maker::value(3);
  let w = wrapper(x);
  0
}
