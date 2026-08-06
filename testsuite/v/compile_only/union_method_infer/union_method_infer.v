// Test that lambda parameter types are inferred through union receivers
// when all members of the union implement the same shape.

shape readable
{
  use cb = (readable, array[u8], usize)->none;

  start(self: self, h: cb): none;
}

source_a
{
  create(): source_a
  {
    new {}
  }

  start(self: source_a, h: readable::cb): none
  {
  }
}

source_b
{
  create(): source_b
  {
    new {}
  }

  start(self: source_b, h: readable::cb): none
  {
  }
}

picker
{
  pick(n: i32): source_a | source_b
  {
    if n == 0
    {
      source_a
    }
    else
    {
      source_b
    }
  }
}

main(): none
{
  // Lambda params should be inferred from readable::cb via the union.
  // The key test is that this compiles: src, data, size have no annotations.
  let a = picker::pick(0);
  a.start (src, data, size) -> {}

  let b = picker::pick(1);
  b.start (src, data, size) -> {}

}
