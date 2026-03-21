find_first_non_space(data: array[u8]): usize
{
  data.pairs (i, c) ->
  {
    if !string::is_space(c)
    {
      raise i
    }
  }

  data.size
}

main(): i32
{
  let padded = array[u8]::fill(3);
  padded(0) = 32;
  padded(1) = 32;
  padded(2) = 97;
  lambda_context_none::find_first_non_space(padded);
  i32 0
}
