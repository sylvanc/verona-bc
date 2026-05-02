// Regression test for float literal encoding/decoding in the bytecode.
//
// The original encoding pipeline used signed-zigzag LEB128 for float
// bit patterns (sleb<int32_t>/sleb<int64_t> after a bit_cast), and the
// reader used static_cast<double>(int64_t) instead of bit_cast.
//
// That broke any float literal whose bit pattern's top "real" bit was
// set, namely anything with magnitude >= 2.0:
//   - sleb writer's `(value << 1) ^ (value >> 63)` overflowed for
//     positive int64s with bit 62 set, producing a negative int64
//     that uleb truncated to a single byte.
//   - the reader's `static_cast<double>(int64_t)` was a numeric
//     conversion, not a bit cast, so even values that encoded
//     correctly came back as huge doubles instead of their literal
//     value.
//
// As a result, literals like 2.5 round-tripped to 0.0, while literals
// like 1.5 round-tripped to ~4.6e18. All that mattered for the test
// suite was "do two distinct literals appear distinct" — which they
// did, by accident — so the bug went undetected.
//
// This test pins down the actual round-trip semantics by checking
// representative literals against known-equivalent expressions.

main(): none
{
  var result = 0;

  // f64 magnitude < 2.0 (bit 62 of int64 cast clear).
  if 1.5 != (3.0 / 2.0) { result = result + 1 }
  if 0.5 != (1.0 / 2.0) { result = result + 2 }

  // f64 magnitude >= 2.0 (bit 62 of int64 cast set) — these used to
  // round-trip to 0.0.
  if 2.0 == 0.0 { result = result + 4 }
  if 2.5 == 0.0 { result = result + 8 }
  if 3.5 == 0.0 { result = result + 16 }
  if 100.0 == 0.0 { result = result + 32 }
  if 2.5 != (5.0 / 2.0) { result = result + 64 }
  if 3.5 != (7.0 / 2.0) { result = result + 128 }

  // Negative f64 (sign bit set in int64 cast).
  if (-2.5) == 0.0 { result = result + 256 }
  if (-2.5) != (-(5.0 / 2.0)) { result = result + 512 }

  // f64 hash agrees with computed equivalents.
  if (2.0).hash != ((4.0 / 2.0)).hash { result = result + 1024 }
  if (2.5).hash != ((5.0 / 2.0)).hash { result = result + 2048 }
  if (3.5).hash == (2.5).hash { result = result + 4096 }

  // f32 same checks.
  if (f32 2.0) == (f32 0.0) { result = result + 8192 }
  if (f32 2.5) == (f32 0.0) { result = result + 16384 }
  if (f32 2.5) != ((f32 5.0) / (f32 2.0)) { result = result + 32768 }
  if (f32 (-2.5)) == (f32 0.0) { result = result + 65536 }

  ffi::exit_code(result)
}
