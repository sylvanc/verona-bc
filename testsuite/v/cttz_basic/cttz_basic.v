main(): none
{
  var result = 0;

  // cttz on u64: counts trailing zero bits.

  // 0 has all 64 bits zero — well-defined as bit width.
  if (u64 0).cttz != 64 { result = result + 1 }

  // 1 = 0b...0001 → 0 trailing zeros.
  if (u64 1).cttz != 0 { result = result + 2 }

  // 2 = 0b...0010 → 1 trailing zero.
  if (u64 2).cttz != 1 { result = result + 4 }

  // 0x10 = 16 → 4 trailing zeros.
  if (u64 0x10).cttz != 4 { result = result + 8 }

  // 0x100 = 256 → 8 trailing zeros.
  if (u64 0x100).cttz != 8 { result = result + 16 }

  // 0x80000000 → 31 trailing zeros.
  if (u64 0x80000000).cttz != 31 { result = result + 32 }

  // High bit only: 1 << 63 → 63 trailing zeros.
  if (u64 1 << 63).cttz != 63 { result = result + 64 }

  // Mask iteration trick: clearing the lowest set bit advances cttz.
  let m = u64 0xC8;
  if m.cttz != 3 { result = result + 128 }
  if (m & (m - 1)).cttz != 6 { result = result + 256 }

  // Other unsigned widths: 0 returns the operand bit width.
  if (u8 0).cttz != 8 { result = result + 512 }
  if (u16 0).cttz != 16 { result = result + 1024 }
  if (u32 0).cttz != 32 { result = result + 2048 }
  if (u8 0x80).cttz != 7 { result = result + 4096 }
  if (u32 0x4000).cttz != 14 { result = result + 8192 }

  // Signed widths: the bit pattern's trailing zeros are returned.
  if (i8 0).cttz != 8 { result = result + 16384 }
  if (i16 0x100).cttz != 8 { result = result + 32768 }
  if (i32 1).cttz != 0 { result = result + 65536 }
  if (i64 -2).cttz != 1 { result = result + 131072 }

  // Platform-width types.
  if (usize 0x20).cttz != 5 { result = result + 262144 }
  if (isize 0x40).cttz != 6 { result = result + 524288 }

  ffi::exit_code result
}

