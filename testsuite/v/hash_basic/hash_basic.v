main(): none
{
  var result = 0;

  // Hash is deterministic: same input → same output.
  if (u64 42).hash != (u64 42).hash { result = result + 1 }
  if "hello".hash != "hello".hash { result = result + 2 }
  if (i32 -7).hash != (i32 -7).hash { result = result + 4 }

  // Distinct values should hash distinctly. mix64 is not a perfect
  // hash but small differences won't collide in practice.
  if (u64 0).hash == (u64 1).hash { result = result + 8 }
  if (u64 1).hash == (u64 2).hash { result = result + 16 }
  if (u64 100).hash == (u64 101).hash { result = result + 32 }

  // u64 0 hashes to mix64(0) = 0 (xor cycles, mul-by-zero).
  if (u64 0).hash != 0 { result = result + 64 }

  // Sequential keys diverge under mix64 (avalanche).
  let h0 = (u64 0).hash;
  let h1 = (u64 1).hash;
  let diff = h0 ^ h1;
  // Expect at least 16 bits of avalanche separation between consecutive keys.
  var bits_changed = 0;
  var mask = u64 1;
  while mask != 0
  {
    if (diff & mask) != 0 { bits_changed = bits_changed + 1 }
    mask = mask << 1
  }
  if bits_changed < 16 { result = result + 128 }

  // Float ±0 normalize to the same hash (matches IEEE-754 ==).
  if (f64 0.0).hash != (-(f64 0.0)).hash { result = result + 256 }
  if (f32 0.0).hash != (-(f32 0.0)).hash { result = result + 512 }

  // Different float values should hash differently in practice.
  if (f64 1.0).hash == (f64 2.0).hash { result = result + 1024 }

  // Bool hashes differ for true/false.
  if true.hash == false.hash { result = result + 2048 }

  // Strings: identical content → identical hash; different → different.
  let a = "hello";
  let b = "hello";
  if a.hash != b.hash { result = result + 4096 }
  if "hello".hash == "world".hash { result = result + 8192 }
  if "".hash == "a".hash { result = result + 16384 }

  // Permutation-sensitive (FNV-1a sees byte order).
  if "ab".hash == "ba".hash { result = result + 32768 }

  // Empty string has the FNV offset basis as its hash.
  if "".hash != 0xcbf29ce484222325 { result = result + 65536 }

  // The free `hash(x)` function dispatches via the to_hash shape.
  if hash(u64 42) != (u64 42).hash { result = result + 131072 }
  if hash("hello") != "hello".hash { result = result + 262144 }

  // Cross-width numeric: i32 and i8 with the same value can hash
  // differently because :::bits sign-extends signed widths and mix64
  // is sensitive to all 64 bits — that's expected and not an error.

  // Sign-distinguishing: i32 -1 and u32 0xFFFFFFFF differ because
  // signed widths sign-extend.
  if (i32 -1).hash == (u32 0xFFFFFFFF).hash { result = result + 524288 }

  ffi::exit_code result
}
