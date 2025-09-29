u16
{
  create(some: u64): u16
  {
    :::convu16(some)
  }

  +(self: u16, other: u16): u16
  {
    :::add(self, other)
  }

  -(self: u16, other: u16): u16
  {
    :::sub(self, other)
  }

  *(self: u16, other: u16): u16
  {
    :::mul(self, other)
  }

  /(self: u16, other: u16): u16
  {
    :::div(self, other)
  }

  %(self: u16, other: u16): u16
  {
    :::mod(self, other)
  }

  &(self: u16, other: u16): u16
  {
    :::and(self, other)
  }

  |(self: u16, other: u16): u16
  {
    :::or(self, other)
  }

  ^(self: u16, other: u16): u16
  {
    :::xor(self, other)
  }

  <<(self: u16, other: u16): u16
  {
    :::shl(self, other)
  }

  >>(self: u16, other: u16): u16
  {
    :::shr(self, other)
  }

  ==(self: u16, other: u16): bool
  {
    :::eq(self, other)
  }

  !=(self: u16, other: u16): bool
  {
    :::ne(self, other)
  }

  <(self: u16, other: u16): bool
  {
    :::lt(self, other)
  }

  <=(self: u16, other: u16): bool
  {
    :::le(self, other)
  }

  >(self: u16, other: u16): bool
  {
    :::gt(self, other)
  }

  >=(self: u16, other: u16): bool
  {
    :::ge(self, other)
  }

  min(self: u16, other: u16): u16
  {
    :::min(self, other)
  }

  max(self: u16, other: u16): u16
  {
    :::max(self, other)
  }

  -(self: u16): u16
  {
    :::neg(self)
  }

  !(self: u16): bool
  {
    :::not(self)
  }

  bool(self: u16): bool
  {
    self != 0.u16
  }

  i8(self: u16): i8
  {
    :::convi8(self)
  }

  i16(self: u16): i16
  {
    :::convi16(self)
  }

  i32(self: u16): i32
  {
    :::convi32(self)
  }

  i64(self: u16): i64
  {
    :::convi64(self)
  }

  u8(self: u16): u8
  {
    :::convu8(self)
  }

  u16(self: u16): u16
  {
    self
  }

  u32(self: u16): u32
  {
    :::convu32(self)
  }

  u64(self: u16): u64
  {
    :::convu64(self)
  }

  ilong(self: u16): ilong
  {
    :::convilong(self)
  }

  ulong(self: u16): ulong
  {
    :::convulong(self)
  }

  isize(self: u16): isize
  {
    :::convisize(self)
  }

  usize(self: u16): usize
  {
    :::convusize(self)
  }

  f32(self: u16): f32
  {
    :::convf32(self)
  }

  f64(self: u16): f64
  {
    :::convf64(self)
  }
}
