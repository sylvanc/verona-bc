std
{
  builtin
  {
    none {}
    bool {}

    i32
    {
      create(x: i32) { x }
    }

    i64 {}
    f32 {}
    f64 {}
  }

  test() {}
}

foo[T1: none = i32, T2: bool = (f32 | f64) | (i32 | i64)]
{
  _f: i32 = 8;

  f1() {}

  f[T3: i32 = i32](a: T3 = 1, b: T1): (T3 & (none | i32), bool)
  {
    if a { 0 } else if b { 1 } else { 2 }

    a and b;

    use Func1 = (()->i32)->T1;
    use Func2 = i32->i32->bool;
    use alias = std;

    var zz = ref a.f;
    zz = ref a.g; // zz is a new ref
    ref zz = 7; // a.g = 7
    zz = 7; // zz is no longer a ref, it's an int
    zz = ref 7;
    let zzz = *zz;

    let yy = (a, (b, let aa)) = 1;

    alias::test(a; b);
    foo::f1();

    a.f = 99;

    let c = i32(1);
    let d = 0.3e6;
    let x = std::test(a.f, 3) b c = 7;
    var y: i32;

    $[none] 3;

    if a b: T1 -> (a, b, true, 0b01, let w);

    (while true {}; a);

    while (a + (b, c))
    {
      -a b + -c d;
      let e = 5;
      a + b;
      continue; // TODO: treat as a terminator
      // return 5;
      // raise;
      throw "Error";
    }

    let e = 7;

    for d (key, value) ->
    {
      key == value;
    }
    else
    {
      "hi"
    }

    // match x
    // {
    //   z -> 3;
    //   y: f64 -> { a / b; 4 }
    // }

    a
  }
}
