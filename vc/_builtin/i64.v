i64
{
  create(some: u64): i64
  {
    :::convi64(some)
  }

  +(self: i64, other: i64): i64
  {
    :::add(self, other)
  }

  -(self: i64, other: i64): i64
  {
    :::sub(self, other)
  }

  *(self: i64, other: i64): i64
  {
    :::mul(self, other)
  }

  /(self: i64, other: i64): i64
  {
    :::div(self, other)
  }

  %(self: i64, other: i64): i64
  {
    :::mod(self, other)
  }

  &(self: i64, other: i64): i64
  {
    :::and(self, other)
  }

  |(self: i64, other: i64): i64
  {
    :::or(self, other)
  }

  ^(self: i64, other: i64): i64
  {
    :::xor(self, other)
  }

  <<(self: i64, other: i64): i64
  {
    :::shl(self, other)
  }

  >>(self: i64, other: i64): i64
  {
    :::shr(self, other)
  }

  ==(self: i64, other: i64): bool
  {
    :::eq(self, other)
  }

  !=(self: i64, other: i64): bool
  {
    :::ne(self, other)
  }

  <(self: i64, other: i64): bool
  {
    :::lt(self, other)
  }

  <=(self: i64, other: i64): bool
  {
    :::le(self, other)
  }

  >(self: i64, other: i64): bool
  {
    :::gt(self, other)
  }

  >=(self: i64, other: i64): bool
  {
    :::ge(self, other)
  }

  min(self: i64, other: i64): i64
  {
    :::min(self, other)
  }

  max(self: i64, other: i64): i64
  {
    :::max(self, other)
  }

  -(self: i64): i64
  {
    :::neg(self)
  }

  !(self: i64): bool
  {
    :::not(self)
  }

  abs(self: i64): i64
  {
    :::abs(self)
  }

  bool(self: i64): bool
  {
    self != 0.i64
  }

  i8(self: i64): i8
  {
    :::convi8(self)
  }

  i16(self: i64): i16
  {
    :::convi16(self)
  }

  i32(self: i64): i32
  {
    :::convi32(self)
  }

  i64(self: i64): i64
  {
    self
  }

  u8(self: i64): u8
  {
    :::convu8(self)
  }

  u16(self: i64): u16
  {
    :::convu16(self)
  }

  u32(self: i64): u32
  {
    :::convu32(self)
  }

  u64(self: i64): u64
  {
    :::convu64(self)
  }

  ilong(self: i64): ilong
  {
    :::convilong(self)
  }

  ulong(self: i64): ulong
  {
    :::convulong(self)
  }

  isize(self: i64): isize
  {
    :::convisize(self)
  }

  usize(self: i64): usize
  {
    :::convusize(self)
  }

  f32(self: i64): f32
  {
    :::convf32(self)
  }

  f64(self: i64): f64
  {
    :::convf64(self)
  }
}
