// Tutorial 3: Generics, Shapes & Arrays
//
// Shapes are structural interfaces — no "implements" needed.
// A class satisfies a shape just by having the right methods.
// Generics are monomorphized (like C++ templates, Rust generics).

// A shape — any class with a score() method satisfies it
shape scorable
{
  score(self: self): i32;
}

// Two classes that independently satisfy "scorable"
task
{
  priority: i32;

  create(priority: i32 = 0): task
  {
    new { priority = priority }
  }

  score(self: task): i32
  {
    self.priority
  }
}

bonus
{
  base: i32;
  multiplier: i32;

  create(base: i32 = 0, multiplier: i32 = 0): bonus
  {
    new { base = base, multiplier = multiplier }
  }

  score(self: bonus): i32
  {
    self.base * self.multiplier
  }
}

// Takes a shape directly — works with any conforming class
get_score(item: scorable): i32
{
  item.score
}

// A generic wrapper class
box[T]
{
  val: T;

  create(val: T): box[T]
  {
    new { val = val }
  }

  get(self: box[T]): T
  {
    self.val
  }
}

main(): i32
{
  var result = 0;

  // -- Shapes: structural subtyping without "implements" --
  let t1 = task(10);
  let t2 = task(20);
  let b1 = bonus(3, 4);

  // Both task and bonus satisfy "scorable" — no declaration needed
  if tutorial3_generics::get_score(t1) != 10 { result = result + 1 }
  if tutorial3_generics::get_score(b1) != 12 { result = result + 2 }

  // -- Generics: type-safe containers --
  let box_int = box[i32](42);
  let box_bool = box[bool](true);

  if box_int.get != 42 { result = result + 4 }
  if !box_bool.get { result = result + 8 }

  // -- Arrays: fixed-size, generic, bounds-checked --
  let arr = array[i32]::fill(5);
  var i = 0;
  while i < arr.size
  {
    arr(i) = i.i32 + 1;
    i = i + 1
  }

  // Sum: 1 + 2 + 3 + 4 + 5 = 15
  var sum: i32 = 0;
  var j = 0;
  while j < arr.size
  {
    sum = sum + arr(j);
    j = j + 1
  }
  if sum != 15 { result = result + 16 }

  // Array literals
  let nums = ::(i32 10, 20, 30);
  if nums(usize 1) != 20 { result = result + 32 }

  // 0 means all checks passed
  result
}
