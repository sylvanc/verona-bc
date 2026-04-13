// Test: is, isnt, and bits from _builtin/is.v

point
{
  x: i32;
  y: i32;
}

main(): none
{
  var result = 0;

  // is/isnt on same object
  let a = point(1, 2);
  if !is(a, a) { result = result + 1 }
  if isnt(a, a) { result = result + 2 }

  // is/isnt on different objects with same values
  let b = point(1, 2);
  if is(a, b) { result = result + 4 }
  if !isnt(a, b) { result = result + 8 }

  // bits on objects: same object gives same bits, different objects differ
  if bits(a) != bits(a) { result = result + 16 }
  if bits(a) == bits(b) { result = result + 32 }

  // bits on object is non-zero (it's a pointer)
  if bits(a) == 0 { result = result + 64 }

  ffi::exit_code result
}
