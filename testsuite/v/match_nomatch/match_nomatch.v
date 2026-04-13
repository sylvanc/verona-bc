// A class with no == method.
noeq
{
  val: i32;
}

main(): none
{
  var result = i32 0;

  // (a) Case value whose type has no == method: should fail to match,
  // not crash. The noeq class has no ==, so TryCallDyn returns nomatch,
  // and the type match arm catches it instead.
  let n1 = noeq(i32 1);
  let a: i32 = (match n1 { (noeq(i32 1)) -> i32 10; (x: noeq) -> x.val; }) else (i32 99);
  if !(a == 1) { result = result + 1 }

  // (b) Case value where == exists but doesn't accept the input type:
  // u32::== takes (u32, u32), so matching an i32 input against a u32
  // case value fails the arg type check. Should fail to match, not crash.
  let b: i32 = (match i32 42
  {
    (u32 42) -> i32 10;
    (x: i32) -> x;
  }) else (i32 0);
  if !(b == 42) { result = result + 2 }

  ffi::exit_code result
}
