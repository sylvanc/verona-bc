// Test typed vars with direct assignment and ref store.
// Uses the bitmask exit code pattern: exit code 0 means all checks passed.

box[T]
{
  value: T;

  create(v: T): box[T]
  {
    new { value = v }
  }

  get(self: box[T]): T
  {
    self.value
  }
}

main(): none
{
  var result = 0;

  // Test 1: typed var with direct assignment.
  var x: i32 = 10;
  if !(x == 10) { result = result + 1 }

  // Test 2: reassign var.
  x = 20;
  if !(x == 20) { result = result + 2 }

  // Test 3: var with inferred type.
  var y = 42;
  if !(y == 42) { result = result + 4 }

  // Test 4: multiple vars.
  var a: i32 = 1;
  var b: i32 = 2;
  var c: i32 = a + b;
  if !(c == 3) { result = result + 8 }

  ffi::exit_code result
}
