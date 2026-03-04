main(): i32
{
  let arr = array[i32]::fill(10);

  arr.pairs (i, v) ->
  {
    arr(i) = i.i32;
    none
  }

  var sum = 0;

  arr.each i ->
  {
    sum = sum + i;
    none
  }

  sum
}
