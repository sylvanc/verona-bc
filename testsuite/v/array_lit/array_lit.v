// Test array literal syntax ::(expr, ...)
// Elements explicitly typed.
main(): i32
{
  let a = ::(i32 5, i32 10, i32 15);
  var sum: i32 = i32 0;
  var index: usize = usize 0;

  while index < a.size
  {
    let val = a(index);
    sum = sum + val;
    index = index + usize 1
  }

  // Expected: 5 + 10 + 15 = 30
  sum
}
