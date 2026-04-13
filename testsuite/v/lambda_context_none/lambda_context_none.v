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

main(): none
{
  var result = 0;

  let padded = array[u8]::fill(3);
  padded(0) = 32;
  padded(1) = 32;
  padded(2) = 97;
  if !lambda_context_none::has_non_space(padded)
  {
    result = result + 1
  }

  let blank = array[u8]::fill(2);
  blank(0) = 32;
  blank(1) = 32;
  if lambda_context_none::has_non_space(blank)
  {
    result = result + 2
  }

  ffi::exit_code result
}
