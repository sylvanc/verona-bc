u8
{
  create(some: u64): u8
  {
    :::convu8(some)
  }

  +(self: u8, other: u8): u8
  {
    :::add(self, other)
  }

  -(self: u8, other: u8): u8
  {
    :::sub(self, other)
  }

  *(self: u8, other: u8): u8
  {
    :::mul(self, other)
  }

  /(self: u8, other: u8): u8
  {
    :::div(self, other)
  }

  %(self: u8, other: u8): u8
  {
    :::mod(self, other)
  }

  &(self: u8, other: u8): u8
  {
    :::and(self, other)
  }

  |(self: u8, other: u8): u8
  {
    :::or(self, other)
  }

  ^(self: u8, other: u8): u8
  {
    :::xor(self, other)
  }

  <<(self: u8, other: u8): u8
  {
    :::shl(self, other)
  }

  >>(self: u8, other: u8): u8
  {
    :::shr(self, other)
  }

  ==(self: u8, other: u8): bool
  {
    :::eq(self, other)
  }

  !=(self: u8, other: u8): bool
  {
    :::ne(self, other)
  }

  <(self: u8, other: u8): bool
  {
    :::lt(self, other)
  }

  <=(self: u8, other: u8): bool
  {
    :::le(self, other)
  }

  >(self: u8, other: u8): bool
  {
    :::gt(self, other)
  }

  >=(self: u8, other: u8): bool
  {
    :::ge(self, other)
  }

  min(self: u8, other: u8): u8
  {
    :::min(self, other)
  }

  max(self: u8, other: u8): u8
  {
    :::max(self, other)
  }

  -(self: u8): u8
  {
    :::neg(self)
  }

  !(self: u8): bool
  {
    :::not(self)
  }

  bool(self: u8): bool
  {
    self != 0.u8
  }

  i8(self: u8): i8
  {
    :::convi8(self)
  }

  i16(self: u8): i16
  {
    :::convi16(self)
  }

  i32(self: u8): i32
  {
    :::convi32(self)
  }

  i64(self: u8): i64
  {
    :::convi64(self)
  }

  u8(self: u8): u8
  {
    self
  }

  u16(self: u8): u16
  {
    :::convu16(self)
  }

  u32(self: u8): u32
  {
    :::convu32(self)
  }

  u64(self: u8): u64
  {
    :::convu64(self)
  }

  ilong(self: u8): ilong
  {
    :::convilong(self)
  }

  ulong(self: u8): ulong
  {
    :::convulong(self)
  }

  isize(self: u8): isize
  {
    :::convisize(self)
  }

  usize(self: u8): usize
  {
    :::convusize(self)
  }

  f32(self: u8): f32
  {
    :::convf32(self)
  }

  f64(self: u8): f64
  {
    :::convf64(self)
  }
}
