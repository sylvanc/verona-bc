class1
{
  class4 {}
}

class2[T]
{
  // ..::T
  use alias1 = T;
}

scope
{
  // ..::class2[..::class1]
  use class2[class1];

  class3
  {
    use alias1;

    // ..::..::..::class1::class4
    f(): class4 {}
  }
}
