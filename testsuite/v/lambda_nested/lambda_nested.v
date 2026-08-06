// Test nested lambdas: a lambda defined inside another lambda.

// Basic nesting: inner lambda captures from outer scope through
// the enclosing lambda's closure (sharing semantics).
nested_capture(): i32
{
  let a: i32 = 42;
  let f = (): i32 -> {
    let g = (): i32 -> a;
    g()
  };
  f()
}

// Triple nesting: three levels of lambda, innermost captures
// from outermost through the chain of closures.
triple_nested(): i32
{
  let x: i32 = 7;
  let f = (): i32 -> {
    let g = (): i32 -> {
      let h = (): i32 -> x;
      h()
    };
    g()
  };
  f()
}

// Sibling lambdas at one level: inc and get each capture count
// independently. Each closure gets its own copy of the captured
// variable, so inc's mutations are NOT visible to get.
make_counter_incorrect(): (() -> i32, () -> i32)
{
  let count: i32 = 0;
  let inc = (): i32 -> { count = count + 1; count };
  let get = (): i32 -> count;
  (inc, get)
}

// Nested sibling lambdas at two levels: the outer lambda captures
// count, then creates two inner lambdas that each independently
// capture count from the outer closure's field. Each inner lambda
// gets its own copy, so inc's mutations are NOT visible to get.
make_nested_counter(): (() -> i32, () -> i32)
{
  let count: i32 = 0;
  let f = (): (() -> i32, () -> i32) -> {
    let inc = (): i32 -> { count = count + 1; count };
    let get = (): i32 -> count;
    (inc, get)
  };
  f()
}

main(): none
{
  var result = 0;

  if lambda_nested::nested_capture() != 42
  {
    result = result + 1
  }

  if lambda_nested::triple_nested() != 7
  {
    result = result + 2
  }

  // Single-level: sibling lambdas do NOT share state.
  // inc has its own copy of count, so get still sees 0.
  (let inc1, let get1) = lambda_nested::make_counter_incorrect();
  inc1();
  inc1();
  inc1();
  if get1() != 0
  {
    result = result + 4
  }

  // Two-level: sibling lambdas also do NOT share state (copy semantics).
  // Each inner lambda independently captures count from the outer closure.
  // inc mutates its own copy, get reads its own copy (always 0).
  (let inc2, let get2) = lambda_nested::make_nested_counter();
  inc2();
  inc2();
  inc2();
  inc2();
  inc2();
  if get2() != 0
  {
    result = result + 8
  }

  ffi::exit_code result
}
