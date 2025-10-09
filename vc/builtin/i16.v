i16
{
  create(some: u64): i16
  {
    :::convi16(some)
  }

  +(self: i16, other: i16): i16
  {
    :::add(self, other)
  }

  -(self: i16, other: i16): i16
  {
    :::sub(self, other)
  }

  *(self: i16, other: i16): i16
  {
    :::mul(self, other)
  }

  /(self: i16, other: i16): i16
  {
    :::div(self, other)
  }

  %(self: i16, other: i16): i16
  {
    :::mod(self, other)
  }

  &(self: i16, other: i16): i16
  {
    :::and(self, other)
  }

  |(self: i16, other: i16): i16
  {
    :::or(self, other)
  }

  ^(self: i16, other: i16): i16
  {
    :::xor(self, other)
  }

  <<(self: i16, other: i16): i16
  {
    :::shl(self, other)
  }

  >>(self: i16, other: i16): i16
  {
    :::shr(self, other)
  }

  ==(self: i16, other: i16): bool
  {
    :::eq(self, other)
  }

  !=(self: i16, other: i16): bool
  {
    :::ne(self, other)
  }

  <(self: i16, other: i16): bool
  {
    :::lt(self, other)
  }

  <=(self: i16, other: i16): bool
  {
    :::le(self, other)
  }

  >(self: i16, other: i16): bool
  {
    :::gt(self, other)
  }

  >=(self: i16, other: i16): bool
  {
    :::ge(self, other)
  }

  min(self: i16, other: i16): i16
  {
    :::min(self, other)
  }

  max(self: i16, other: i16): i16
  {
    :::max(self, other)
  }

  -(self: i16): i16
  {
    :::neg(self)
  }

  !(self: i16): bool
  {
    :::not(self)
  }

  abs(self: i16): i16
  {
    :::abs(self)
  }

  bool(self: i16): bool
  {
    self != 0.i16
  }

  i8(self: i16): i8
  {
    :::convi8(self)
  }

  i16(self: i16): i16
  {
    self
  }

  i32(self: i16): i32
  {
    :::convi32(self)
  }

  i64(self: i16): i64
  {
    :::convi64(self)
  }

  u8(self: i16): u8
  {
    :::convu8(self)
  }

  u16(self: i16): u16
  {
    :::convu16(self)
  }

  u32(self: i16): u32
  {
    :::convu32(self)
  }

  u64(self: i16): u64
  {
    :::convu64(self)
  }

  ilong(self: i16): ilong
  {
    :::convilong(self)
  }

  ulong(self: i16): ulong
  {
    :::convulong(self)
  }

  isize(self: i16): isize
  {
    :::convisize(self)
  }

  usize(self: i16): usize
  {
    :::convusize(self)
  }

  f32(self: i16): f32
  {
    :::convf32(self)
  }

  f64(self: i16): f64
  {
    :::convf64(self)
  }
}
