i32 {}

Class0[T1: none = i32, T2: bool = (f32 | f64) | (i32 | i64)]
{
  _f: i32 = 8;

  f1() {}

  f[T3: i32 = i32](a: T3 = 1, b: T1): (T3 & (none | i32), bool)
  {
    use std::builtin;
    use Func1 = (()->i32)->T1;
    use Func2 = i32->i32->bool;

    let x = std::test(a.f) b c = 7;
    var y: i32;

    $[hi] 3;

    if a b: T1 -> (a, b, true, 0b01, let w);

    (while true {}; a);

    while (a + b)
    {
      -a b + -c d;
      a + b;
      continue;
      return 5;
      raise;
      throw "Error";
    }

    for container (key, value) ->
    {
      key == value;
    }
    else
    {
      "hi"
    }

    match x
    {
      z -> 3;
      y: f64 -> { a / b; 4 }
    }

    a
  }
}
