// Tutorial 2: Classes & Objects
//
// Classes use bare names — no "class" keyword.
// Fields end with semicolons.
// If no create() is defined, one is auto-generated from fields.

// A 2D point class
point
{
  x: i32;
  y: i32;

  // Method — first param is self
  manhattan(self: point): i32
  {
    abs(self.x) + abs(self.y)
  }

  // Operator overloading — operators are just methods
  +(self: point, other: point): point
  {
    point(self.x + other.x, self.y + other.y)
  }

  ==(self: point, other: point): bool
  {
    (self.x == other.x) & (self.y == other.y)
  }
}

// A helper — free functions work too
abs(x: i32): i32
{
  if x < 0 { -x } else { x }
}

// A cell with a default argument
cell
{
  value: i32;

  create(value: i32 = 0): cell
  {
    new { value = value }
  }
}

main(): i32
{
  // Constructor sugar: point(3, 4) calls point::create(3, 4)
  let a = point(3, 4);
  let b = point(1, 2);

  // Method call with dot syntax
  let d = a.manhattan;

  // Operator calls
  let c = a + b;

  // Field mutation — let constrains the binding, not the object
  let p = point(0, 0);
  p.x = 10;
  p.x = p.x + 5

  // Default arguments
  let empty = cell;
  let full = cell(42);

  var result = 0;
  if d != 7 { result = result + 1 }
  if !(c == point(4, 6)) { result = result + 2 }
  if p.x != 10 { result = result + 4 }
  if empty.value != 0 { result = result + 8 }
  if full.value != 42 { result = result + 16 }

  // 0 means all checks passed
  result
}
