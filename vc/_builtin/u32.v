u32
{
  create(some: u64): u32
  {
    :::convu32(some)
  }

  +(self: u32, other: u32): u32
  {
    :::add(self, other)
  }

  -(self: u32, other: u32): u32
  {
    :::sub(self, other)
  }

  *(self: u32, other: u32): u32
  {
    :::mul(self, other)
  }

  /(self: u32, other: u32): u32
  {
    :::div(self, other)
  }

  %(self: u32, other: u32): u32
  {
    :::mod(self, other)
  }

  &(self: u32, other: u32): u32
  {
    :::and(self, other)
  }

  |(self: u32, other: u32): u32
  {
    :::or(self, other)
  }

  ^(self: u32, other: u32): u32
  {
    :::xor(self, other)
  }

  <<(self: u32, other: u32): u32
  {
    :::shl(self, other)
  }

  >>(self: u32, other: u32): u32
  {
    :::shr(self, other)
  }

  ==(self: u32, other: u32): bool
  {
    :::eq(self, other)
  }

  !=(self: u32, other: u32): bool
  {
    :::ne(self, other)
  }

  <(self: u32, other: u32): bool
  {
    :::lt(self, other)
  }

  <=(self: u32, other: u32): bool
  {
    :::le(self, other)
  }

  >(self: u32, other: u32): bool
  {
    :::gt(self, other)
  }

  >=(self: u32, other: u32): bool
  {
    :::ge(self, other)
  }

  min(self: u32, other: u32): u32
  {
    :::min(self, other)
  }

  max(self: u32, other: u32): u32
  {
    :::max(self, other)
  }

  -(self: u32): u32
  {
    :::neg(self)
  }

  !(self: u32): bool
  {
    :::not(self)
  }

  bool(self: u32): bool
  {
    self != 0.u32
  }

  i8(self: u32): i8
  {
    :::convi8(self)
  }

  i16(self: u32): i16
  {
    :::convi16(self)
  }

  i32(self: u32): i32
  {
    :::convi32(self)
  }

  i64(self: u32): i64
  {
    :::convi64(self)
  }

  u8(self: u32): u8
  {
    :::convu8(self)
  }

  u16(self: u32): u16
  {
    :::convu16(self)
  }

  u32(self: u32): u32
  {
    self
  }

  u64(self: u32): u64
  {
    :::convu64(self)
  }

  ilong(self: u32): ilong
  {
    :::convilong(self)
  }

  ulong(self: u32): ulong
  {
    :::convulong(self)
  }

  isize(self: u32): isize
  {
    :::convisize(self)
  }

  usize(self: u32): usize
  {
    :::convusize(self)
  }

  f32(self: u32): f32
  {
    :::convf32(self)
  }

  f64(self: u32): f64
  {
    :::convf64(self)
  }
}
