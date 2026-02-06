u64
{
  create(some: u64): u64
  {
    some
  }

  +(self: u64, other: u64): u64
  {
    :::add(self, other)
  }

  -(self: u64, other: u64): u64
  {
    :::sub(self, other)
  }

  *(self: u64, other: u64): u64
  {
    :::mul(self, other)
  }

  /(self: u64, other: u64): u64
  {
    :::div(self, other)
  }

  %(self: u64, other: u64): u64
  {
    :::mod(self, other)
  }

  &(self: u64, other: u64): u64
  {
    :::and(self, other)
  }

  |(self: u64, other: u64): u64
  {
    :::or(self, other)
  }

  ^(self: u64, other: u64): u64
  {
    :::xor(self, other)
  }

  <<(self: u64, other: u64): u64
  {
    :::shl(self, other)
  }

  >>(self: u64, other: u64): u64
  {
    :::shr(self, other)
  }

  ==(self: u64, other: u64): bool
  {
    :::eq(self, other)
  }

  !=(self: u64, other: u64): bool
  {
    :::ne(self, other)
  }

  <(self: u64, other: u64): bool
  {
    :::lt(self, other)
  }

  <=(self: u64, other: u64): bool
  {
    :::le(self, other)
  }

  >(self: u64, other: u64): bool
  {
    :::gt(self, other)
  }

  >=(self: u64, other: u64): bool
  {
    :::ge(self, other)
  }

  min(self: u64, other: u64): u64
  {
    :::min(self, other)
  }

  max(self: u64, other: u64): u64
  {
    :::max(self, other)
  }

  -(self: u64): u64
  {
    :::neg(self)
  }

  !(self: u64): bool
  {
    :::not(self)
  }

  bool(self: u64): bool
  {
    self != 0.u64
  }

  i8(self: u64): i8
  {
    :::convi8(self)
  }

  i16(self: u64): i16
  {
    :::convi16(self)
  }

  i32(self: u64): i32
  {
    :::convi32(self)
  }

  i64(self: u64): i64
  {
    :::convi64(self)
  }

  u8(self: u64): u8
  {
    :::convu8(self)
  }

  u16(self: u64): u16
  {
    :::convu16(self)
  }

  u32(self: u64): u32
  {
    :::convu32(self)
  }

  u64(self: u64): u64
  {
    self
  }

  ilong(self: u64): ilong
  {
    :::convilong(self)
  }

  ulong(self: u64): ulong
  {
    :::convulong(self)
  }

  isize(self: u64): isize
  {
    :::convisize(self)
  }

  usize(self: u64): usize
  {
    :::convusize(self)
  }

  f32(self: u64): f32
  {
    :::convf32(self)
  }

  f64(self: u64): f64
  {
    :::convf64(self)
  }
}
