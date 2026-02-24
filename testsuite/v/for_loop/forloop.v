main(): i32
{
  var arr = array[i32](10);
  var index = 0;

  while index < arr.size
  {
    arr(index) = index;
    index = index + 1;
  }

  var sum = 0;

  for arr.values() i ->
  {
    sum = sum + i;
  }

  // Returns 45, the sum of 0..9.
  sum
}
