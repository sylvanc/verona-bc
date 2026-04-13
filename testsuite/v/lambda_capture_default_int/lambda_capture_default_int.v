main(): none
{
  let count = 0;
  let f = (): none ->
  {
    if count < 5
    {
      count = count + 1
    }
  }

  f();
}
