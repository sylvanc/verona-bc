i8
{
  create(some: i8 = 0): i8
  {
    some
  }

  +(self: i8, other: i8): i8
  {
    :::add(self, other)
  }

  -(self: i8, other: i8): i8
  {
    :::sub(self, other)
  }

  *(self: i8, other: i8): i8
  {
    :::mul(self, other)
  }

  /(self: i8, other: i8): i8
  {
    :::div(self, other)
  }

  %(self: i8, other: i8): i8
  {
    :::mod(self, other)
  }

  &(self: i8, other: i8): i8
  {
    :::and(self, other)
  }

  |(self: i8, other: i8): i8
  {
    :::or(self, other)
  }

  ^(self: i8, other: i8): i8
  {
    :::xor(self, other)
  }

  <<(self: i8, other: i8): i8
  {
    :::shl(self, other)
  }

  >>(self: i8, other: i8): i8
  {
    :::shr(self, other)
  }

  ==(self: i8, other: i8): bool
  {
    :::eq(self, other)
  }

  !=(self: i8, other: i8): bool
  {
    :::ne(self, other)
  }

  <(self: i8, other: i8): bool
  {
    :::lt(self, other)
  }

  <=(self: i8, other: i8): bool
  {
    :::le(self, other)
  }

  >(self: i8, other: i8): bool
  {
    :::gt(self, other)
  }

  >=(self: i8, other: i8): bool
  {
    :::ge(self, other)
  }

  min(self: i8, other: i8): i8
  {
    :::min(self, other)
  }

  max(self: i8, other: i8): i8
  {
    :::max(self, other)
  }

  -(self: i8): i8
  {
    :::neg(self)
  }

  !(self: i8): i8
  {
    :::not(self)
  }

  abs(self: i8): i8
  {
    :::abs(self)
  }

  bool(self: i8): bool
  {
    self != 0
  }

  i8(self: i8): i8
  {
    self
  }

  i16(self: i8): i16
  {
    :::convi16(self)
  }

  i32(self: i8): i32
  {
    :::convi32(self)
  }

  i64(self: i8): i64
  {
    :::convi64(self)
  }

  u8(self: i8): u8
  {
    :::convu8(self)
  }

  u16(self: i8): u16
  {
    :::convu16(self)
  }

  u32(self: i8): u32
  {
    :::convu32(self)
  }

  u64(self: i8): u64
  {
    :::convu64(self)
  }

  ilong(self: i8): ilong
  {
    :::convilong(self)
  }

  ulong(self: i8): ulong
  {
    :::convulong(self)
  }

  isize(self: i8): isize
  {
    :::convisize(self)
  }

  usize(self: i8): usize
  {
    :::convusize(self)
  }

  f32(self: i8): f32
  {
    :::convf32(self)
  }

  f64(self: i8): f64
  {
    :::convf64(self)
  }
}
