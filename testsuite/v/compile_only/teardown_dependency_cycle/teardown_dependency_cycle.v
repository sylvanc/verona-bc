// No teardown order can satisfy mutually dependent callbacks. Each callback
// calls the other initialized library, creating opposing lifetime edges.
// The compiler must reject the cycle; missing either edge would let it compile.

library_b
{
  touch(): none
  {
    :::touch_b(0);
  }

  use
  {
    init(): any
    {
      { library_a::touch(); }
    }

    touch_b = "printval"(any): none;
  }
}

library_a
{
  touch(): none
  {
    :::touch_a(0);
  }

  use
  {
    init(): any
    {
      { library_b::touch(); }
    }

    touch_a = "printval"(any): none;
  }
}

main(): none
{
  library_a::touch();
}
