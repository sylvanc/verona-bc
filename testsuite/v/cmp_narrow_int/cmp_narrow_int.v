// Comparison ops on narrow integer types (u8/i8/u16/i16) must produce
// Bool values, not silently coerce the bool result to the operand
// type. Regression test: pre-fix, `u8 < u8` produced a U8-tagged Value
// (1 or 0), which then failed at any boolean use site
// (Cond / `let b: bool = ...`).

main(): none
{
  var result: i32 = 0;

  // u8 ordering — needs all four ops since binop's type cast applied
  // to all of lt/le/gt/ge.
  let a: u8 = u8 1;
  let b: u8 = u8 2;
  if !(a < b) { result = result + 1 }
  if !(a <= b) { result = result + 2 }
  if !(b > a) { result = result + 4 }
  if !(b >= a) { result = result + 8 }

  // i8 ordering — same coercion bug.
  let c: i8 = i8 1;
  let d: i8 = i8 2;
  if !(c < d) { result = result + 16 }

  // u16 / i16 also affected by the narrow-int cast.
  let e: u16 = u16 1;
  let f: u16 = u16 2;
  if !(e < f) { result = result + 32 }

  let g: i16 = i16 1;
  let h: i16 = i16 2;
  if !(g < h) { result = result + 64 }

  // Wider ints worked before the fix; keep them in to lock in behavior.
  let i: i32 = 1;
  let j: i32 = 2;
  if !(i < j) { result = result + 128 }

  let k: u64 = u64 1;
  let l: u64 = u64 2;
  if !(k < l) { result = result + 256 }

  let m: f64 = f64 1.0;
  let n: f64 = f64 2.0;
  if !(m < n) { result = result + 512 }

  ffi::exit_code result
}
