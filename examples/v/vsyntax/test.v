some
{
  test(a, b) {}
}

foo[T1 = i32, T2 = (f32 | f64) | (i32 | i64)]
  where (T1 < none) & (T2 < bool)
{
  f: i32; // = 8;
  g;

  *(a) {}
  $(a) {}
  +(a, b, c) {}
  apply(a, b) {}
  ref apply(a, b, c) {}

  f1() {}

  f2[T3 = i32](a: T3, b: T1 = 1): (T3 & (none | i32), bool)
    where T3 < i32
  {
    if a { 0 } else if b { 1 } else { 2 }

    a & b;

    use Func1 = (()->i32)->T1;
    use Func2 = i32->i32->bool;
    use alias = some;
    use some;

    var zz = ref a.f;
    zz = ref a.g; // zz is a new ref
    ref zz = 7; // a.g = 7
    zz = 7; // zz is no longer a ref, it's an int
    zz = ref 7;
    let zzz = *zz;

    var q0 = (var q1 = 3) = 4;

    var aa = 1;
    var bb = 2;
    let yy = (aa, (bb, var cc = 3)) = 1;

    alias::test((a; b), zz);
    some::test((a; b), zz);
    foo::f1();

    a.f = 99;

    let c = i32(1);
    let d = 0.3e6;
    let x = some::test(a.f, 3) b c = 7;
    var y: i32;

    $[none] 3;

    // if a b: T1 -> (a, b, true, 0b01, let w);

    (while true {}; a);

    while (a + (b, c))
    {
      if a
      {
        continue
      }
      else
      {
        break
      }
      // return 5;
      // raise;
      // throw "Error";
    }

    let e = 7;

    // for d (key, value) ->
    // {
    //   key == value;
    // }
    // else
    // {
    //   "hi"
    // }

    // match x
    // {
    //   z -> 3;
    //   y: f64 -> { a / b; 4 }
    // }

    a
  }
}

main(): i32
{
  i32 4
}
