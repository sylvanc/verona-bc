// Test type inference through Store (field assignment).
// When storing a value through a ref[T], the stored value's
// default type should be refined to T.

box
{
  x: i32;

  create(x: i32 = i32 0): box
  {
    new {x}
  }
}

main(): i32
{
  let b = box;

  // Store refinement: 42 (default u64) should be refined to i32
  // because b.x is i32.
  b.x = 42;

  // Verify the stored value.
  var result = 0;

  if b.x == i32 42
  {
    result = result + 1;
  }

  // Store refinement with a different value.
  b.x = 10;

  if b.x == i32 10
  {
    result = result + 2;
  }

  // result should be 3 (both tests pass).
  result
}
