// Stress test for type inference with raising lambdas.
// Exercises:
//   1. Literal inference from enclosing function return type through raise
//   2. Raise in if/else branches with inferred literal types
//   3. Lambda that always raises (return type inferred to none)
//   4. Raise with a computed value that needs inference
//   5. Generic function with raise inference
//   6. Backward refinement through raise

// --- Test 1: Basic literal inference through raise ---
// The `0` in `raise 0` should be inferred as i32 from the return type of
// raise_literal, not left as default u64.
raise_literal(flag: i32): i32
{
  let f = (x: i32) -> {
    if x == 0
    {
      raise 0
    }
  };
  f(flag);
  42
}

// --- Test 2: Raise in both branches with different inferred values ---
// Both `1` and `2` should be inferred as i32 from enclosing return type.
raise_branches(flag: i32): i32
{
  let f = (x: i32) -> {
    if x == 0
    {
      raise 1
    }
    else
    {
      raise 2
    }
  };
  f(flag);
  0
}

// --- Test 3: Always-raising lambda (lambda return type -> none) ---
// The lambda always raises, so its own return type should be none.
// The function still returns i32 via the raise.
always_raises(val: i32): i32
{
  let f = (x: i32) -> {
    raise x
  };
  f(val);
  0
}

// --- Test 4: Raise with computed value needing inference ---
// `a + b` are default-typed; the raise should infer them as i32.
raise_computed(a: i32, b: i32): i32
{
  let f = () -> {
    raise a + b
  };
  f();
  0
}

// --- Test 5: Generic function with raise inference ---
// T is i32, so the raised `0` should be inferred as i32.
raise_generic[T](val: T, zero: T): T
{
  let f = (x: T) -> {
    if x == zero
    {
      raise zero
    }
  };
  f(val);
  val
}

// --- Test 6: Raise after variable assignment with inference ---
// The variable `result` should be inferred as i32 from the raise.
raise_via_var(flag: i32): i32
{
  let f = (x: i32) -> {
    var result: i32 = x + 1;
    raise result
  };
  f(flag);
  0
}

main(): i32
{
  var result = 0;

  // Test 1: raise_literal(0) should raise 0 (early return), not 42
  if raise_infer::raise_literal(0) != 0
  {
    result = result + 1
  }

  // Test 2: raise_branches(0) should raise 1
  if raise_infer::raise_branches(0) != 1
  {
    result = result + 2
  }

  // Test 2b: raise_branches(1) should raise 2
  if raise_infer::raise_branches(1) != 2
  {
    result = result + 4
  }

  // Test 3: always_raises(7) should raise 7
  if raise_infer::always_raises(7) != 7
  {
    result = result + 8
  }

  // Test 4: raise_computed(10, 5) should raise 15
  if raise_infer::raise_computed(10, 5) != 15
  {
    result = result + 16
  }

  // Test 5: raise_generic[i32](0, 0) should raise 0 (early return)
  if raise_infer::raise_generic[i32](0, 0) != 0
  {
    result = result + 32
  }

  // Test 5b: raise_generic[i32](5, 0) should return 5 (no raise)
  if raise_infer::raise_generic[i32](5, 0) != 5
  {
    result = result + 64
  }

  // Test 6: raise_via_var(9) should raise 10 (9 + 1)
  if raise_infer::raise_via_var(9) != 10
  {
    result = result + 128
  }

  result
}
