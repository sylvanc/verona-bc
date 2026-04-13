main(): none
{
  let arr = array[i32]::fill(10);

  arr.pairs (i, v) ->
  {
    arr(i) = i.i32
  }

  var sum = 0;

  arr.each i ->
  {
    sum = sum + i
  }

  ffi::exit_code sum
}
