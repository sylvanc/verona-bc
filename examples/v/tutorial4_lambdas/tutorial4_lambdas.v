// Tutorial 4: Lambdas & Higher-Order Functions
//
// Lambdas are anonymous functions: (params) -> { body }
// They capture variables from the enclosing scope (closures).
// Function types like "i32 -> i32" are actually shapes with apply().

// A higher-order function — takes a function as an argument
apply_twice(f: i32 -> i32, x: i32): i32
{
  f(f(x))
}

// Returns a closure that remembers its state
make_counter(): () -> i32
{
  let count: i32 = 0;
  (): i32 -> { count = count + 1; count }
}

// Generic higher-order function: transform each element
map_array[T](arr: array[T], f: T -> T): array[T]
{
  let result = array[T]::fill(arr.size);
  var i = 0;

  while i < arr.size
  {
    result(i) = f(arr(i));
    i = i + 1
  }

  result
}

main(): i32
{
  var result = 0;

  // Lambda expression
  let double = (x: i32): i32 -> { x + x };
  if double(5) != 10 { result = result + 1 }

  // Passing a lambda to a higher-order function
  let r = tutorial4_lambdas::apply_twice(double, 3);
  if r != 12 { result = result + 2 }

  // Stateful closure — each call increments
  let counter = tutorial4_lambdas::make_counter();
  let c1 = counter();
  let c2 = counter();
  let c3 = counter();
  if (c1 != 1) | (c2 != 2) | (c3 != 3)
  {
    result = result + 4
  }

  // Transform an array with a lambda
  let nums = ::(i32 1, 2, 3, 4, 5);

  let squared = tutorial4_lambdas::map_array(
    nums,
    (x: i32): i32 -> { x * x }
  );

  // Sum the squared values: 1 + 4 + 9 + 16 + 25 = 55
  var sum: i32 = 0;
  var i = 0;
  while i < squared.size
  {
    sum = sum + squared(i);
    i = i + 1
  }
  if sum != 55 { result = result + 8 }

  // 0 means all checks passed
  result
}
