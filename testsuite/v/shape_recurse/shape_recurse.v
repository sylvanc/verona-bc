// Test: a shape whose function returns the shape type (not self).
// Verifies no infinite recursion in check_shape_subtype when
// checking box <: returner, since returner.get returns returner
// (a shape), which triggers a recursive subtype check.
shape returner
{
  get(self: self): returner;
  val(self: self): i32;
}

box
{
  v: i32;

  create(x: i32): box
  {
    new { v = x }
  }

  get(self: box): box
  {
    self
  }

  val(self: box): i32
  {
    self.v
  }
}

invoke(r: returner): i32
{
  r.val()
}

main(): none
{
  let b = box::create(42);
  ffi::exit_code(shape_recurse::invoke(b))
}
