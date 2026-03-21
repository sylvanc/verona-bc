has_non_space(data: array[u8]): bool
{
  data.pairs (i, c) ->
  {
    if !string::is_space(c)
    {
      raise true
    }
  }

  false
}

main(): i32
{
  let padded = array[u8]::fill(3);
  padded(0) = 32;
  padded(1) = 32;
  padded(2) = 97;
  if !lambda_context_none::has_non_space(padded)
  {
    return 1
  }

  let blank = array[u8]::fill(2);
  blank(0) = 32;
  blank(1) = 32;
  if lambda_context_none::has_non_space(blank)
  {
    return 2
  }

  0
}
