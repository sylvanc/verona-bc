add(a: i32, b: i32): i32
{
  a + b
}

sub(a: i32, b: i32): i32
{
  a - b
}

three_args(a: i32, b: i32, c: i32): i32
{
  a + b + c
}

box
{
  val: i32;

  create(val: i32): box { new { val = val } }
  add(self: box, b: i32): i32 { self.val + b }
  combine(self: box, a: i32, b: i32): i32 { self.val + a + b }
}

main(): i32
{
  var result = 0;

  // Partial application: fix first argument.
  let add5 = partial_app::add(5, _);
  if add5(3) != 8
  {
    result = result + 1
  }

  // Partial application: fix second argument.
  let sub3 = partial_app::sub(_, 3);
  if sub3(10) != 7
  {
    result = result + 2
  }

  // Multiple placeholders.
  let f = partial_app::three_args(_, 10, _);
  if f(1, 2) != 13
  {
    result = result + 4
  }

  // All placeholders.
  let g = partial_app::add(_, _);
  if g(4, 5) != 9
  {
    result = result + 8
  }

  // Dynamic call partial application: fix the placeholder.
  let obj = box(100);
  let h = obj.add(_);
  if h(5) != 105
  {
    result = result + 16
  }

  // Dynamic call partial application: fix first arg.
  let j = obj.combine(10, _);
  if j(20) != 130
  {
    result = result + 32
  }

  // Dynamic call partial application: fix second arg.
  let k = obj.combine(_, 20);
  if k(10) != 130
  {
    result = result + 64
  }

  // Dynamic call partial application: all placeholders.
  let m = obj.combine(_, _);
  if m(10, 20) != 130
  {
    result = result + 128
  }

  result
}
