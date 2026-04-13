// Tests bulk array operations across primitive types, object types,
// overlapping ranges, and string operations that use them.

box
{
  val: i32;

  create(val: i32): box
  {
    new {val}
  }
}

main(): none
{
  var result = 0;

  // === u8 copy ===
  let u8_src = array[u8]::fill 5;
  u8_src(0) = 10; u8_src(1) = 20; u8_src(2) = 30;
  u8_src(3) = 40; u8_src(4) = 50;

  let u8_dst = array[u8]::fill 5;
  u8_dst.copy_from(0, u8_src, 0, 5);
  if u8_dst(0) != 10 { result = result + 1 }
  if u8_dst(4) != 50 { result = result + 2 }

  // === i32 copy, fill, compare ===
  let i32_src = array[i32]::fill(4, 10);
  let i32_dst = array[i32]::fill 4;
  i32_dst.copy_from(0, i32_src, 0, 4);
  if i32_dst(0) != 10 { result = result + 4 }
  if i32_dst(3) != 10 { result = result + 8 }

  i32_dst.fill_range(1, 2, 99);
  if i32_dst(0) != 10 { result = result + 16 }
  if i32_dst(1) != 99 { result = result + 32 }
  if i32_dst(2) != 99 { result = result + 64 }
  if i32_dst(3) != 10 { result = result + 128 }

  let i32_a = array[i32]::fill(3, 5);
  let i32_b = array[i32]::fill(3, 5);
  if i32_a.compare(0, i32_b, 0, 3) != i64 0 { result = result + 256 }

  // === f64 copy and fill ===
  let f64_src = array[f64]::fill(3, 1.5);
  let f64_dst = array[f64]::fill(3, 0.0);
  f64_dst.copy_from(0, f64_src, 0, 3);
  if f64_dst(0) != 1.5 { result = result + 512 }
  if f64_dst(2) != 1.5 { result = result + 1024 }

  f64_dst.fill_range(usize 0, usize 2, 3.14);
  if f64_dst(0) != 3.14 { result = result + 2048 }
  if f64_dst(2) != 1.5 { result = result + 4096 }

  // === usize copy ===
  let usize_src = array[usize]::fill(3, usize 42);
  let usize_dst = array[usize]::fill 3;
  usize_dst.copy_from(0, usize_src, 0, 3);
  if usize_dst(0) != usize 42 { result = result + 8192 }
  if usize_dst(2) != usize 42 { result = result + 16384 }

  // === Overlapping copy (forward: dst > src) ===
  let overlap = array[i32]::fill 6;
  overlap(0) = 1; overlap(1) = 2; overlap(2) = 3;
  overlap(3) = 4; overlap(4) = 5; overlap(5) = 6;
  overlap.copy_from(2, overlap, 0, 3);
  if overlap(2) != 1 { result = result + 32768 }
  if overlap(3) != 2 { result = result + 65536 }
  if overlap(4) != 3 { result = result + 131072 }

  // === Overlapping copy (backward: dst < src) ===
  let overlap2 = array[i32]::fill 6;
  overlap2(0) = 1; overlap2(1) = 2; overlap2(2) = 3;
  overlap2(3) = 4; overlap2(4) = 5; overlap2(5) = 6;
  overlap2.copy_from(0, overlap2, 2, 3);
  if overlap2(0) != 3 { result = result + 262144 }
  if overlap2(1) != 4 { result = result + 524288 }
  if overlap2(2) != 5 { result = result + 1048576 }

  // === Object array copy ===
  let obj_src = array[box]::fill(3, box(0));
  obj_src(0) = box(10); obj_src(1) = box(20); obj_src(2) = box(30);
  let obj_dst = array[box]::fill(3, box(0));
  obj_dst.copy_from(0, obj_src, 0, 3);
  if obj_dst(0).val != 10 { result = result + 2097152 }
  if obj_dst(1).val != 20 { result = result + 4194304 }
  if obj_dst(2).val != 30 { result = result + 8388608 }

  // Source unchanged after copy.
  if obj_src(0).val != 10 { result = result + 16777216 }

  // === Object array fill ===
  let obj_filled = array[box]::fill(3, box(0));
  obj_filled.fill_range(0, 3, box(99));
  if obj_filled(0).val != 99 { result = result + 33554432 }
  if obj_filled(2).val != 99 { result = result + 67108864 }

  // === Zero-length operations (no-ops) ===
  u8_dst.copy_from(0, u8_src, 0, 0);
  if u8_dst(0) != 10 { result = result + 134217728 }

  if u8_src.compare(0, u8_dst, 0, 0) != i64 0 { result = result + 268435456 }

  // === Overlapping copy on object arrays (forward: dst > src) ===
  let obj_overlap = array[box]::fill(5, box(0));
  obj_overlap(0) = box(1); obj_overlap(1) = box(2); obj_overlap(2) = box(3);
  obj_overlap(3) = box(4); obj_overlap(4) = box(5);
  obj_overlap.copy_from(2, obj_overlap, 0, 3);
  if obj_overlap(2).val != 1 { result = result + 536870912 }
  if obj_overlap(3).val != 2 { result = result + 1073741824 }

  ffi::exit_code(i32 result)
}
