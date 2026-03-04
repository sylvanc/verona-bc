// Test: circular dependency between once functions should be a compile error.

once f(): i32
{
  memo_cycle::g()
}

once g(): i32
{
  memo_cycle::f()
}

main(): i32
{
  memo_cycle::f()
}
