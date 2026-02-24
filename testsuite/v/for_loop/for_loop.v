main(): i32
{
  var arr = array[i32](10);
  var index = 0;

  while index < arr.size
  {
    arr(index) = index.i32;
    index = index + 1
  }

  var sum = 0;

  for arr.values() i ->
  {
    sum = sum + i
  }

  sum
}
