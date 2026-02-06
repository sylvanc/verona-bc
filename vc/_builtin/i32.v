i32
{
  create(some: u64): i32
  {
    :::convi32(some)
  }

  +(self: i32, other: i32): i32
  {
    :::add(self, other)
  }

  -(self: i32, other: i32): i32
  {
    :::sub(self, other)
  }

  *(self: i32, other: i32): i32
  {
    :::mul(self, other)
  }

  /(self: i32, other: i32): i32
  {
    :::div(self, other)
  }

  %(self: i32, other: i32): i32
  {
    :::mod(self, other)
  }

  &(self: i32, other: i32): i32
  {
    :::and(self, other)
  }

  |(self: i32, other: i32): i32
  {
    :::or(self, other)
  }

  ^(self: i32, other: i32): i32
  {
    :::xor(self, other)
  }

  <<(self: i32, other: i32): i32
  {
    :::shl(self, other)
  }

  >>(self: i32, other: i32): i32
  {
    :::shr(self, other)
  }

  ==(self: i32, other: i32): bool
  {
    :::eq(self, other)
  }

  !=(self: i32, other: i32): bool
  {
    :::ne(self, other)
  }

  <(self: i32, other: i32): bool
  {
    :::lt(self, other)
  }

  <=(self: i32, other: i32): bool
  {
    :::le(self, other)
  }

  >(self: i32, other: i32): bool
  {
    :::gt(self, other)
  }

  >=(self: i32, other: i32): bool
  {
    :::ge(self, other)
  }

  min(self: i32, other: i32): i32
  {
    :::min(self, other)
  }

  max(self: i32, other: i32): i32
  {
    :::max(self, other)
  }

  -(self: i32): i32
  {
    :::neg(self)
  }

  !(self: i32): bool
  {
    :::not(self)
  }

  abs(self: i32): i32
  {
    :::abs(self)
  }

  bool(self: i32): bool
  {
    self != 0.i32
  }

  i8(self: i32): i8
  {
    :::convi8(self)
  }

  i16(self: i32): i16
  {
    :::convi16(self)
  }

  i32(self: i32): i32
  {
    self
  }

  i64(self: i32): i64
  {
    :::convi64(self)
  }

  u8(self: i32): u8
  {
    :::convu8(self)
  }

  u16(self: i32): u16
  {
    :::convu16(self)
  }

  u32(self: i32): u32
  {
    :::convu32(self)
  }

  u64(self: i32): u64
  {
    :::convu64(self)
  }

  ilong(self: i32): ilong
  {
    :::convilong(self)
  }

  ulong(self: i32): ulong
  {
    :::convulong(self)
  }

  isize(self: i32): isize
  {
    :::convisize(self)
  }

  usize(self: i32): usize
  {
    :::convusize(self)
  }

  f32(self: i32): f32
  {
    :::convf32(self)
  }

  f64(self: i32): f64
  {
    :::convf64(self)
  }
}
