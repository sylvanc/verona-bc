ulong
{
  create(some: u64): ulong
  {
    :::convulong(some)
  }

  +(self: ulong, other: ulong): ulong
  {
    :::add(self, other)
  }

  -(self: ulong, other: ulong): ulong
  {
    :::sub(self, other)
  }

  *(self: ulong, other: ulong): ulong
  {
    :::mul(self, other)
  }

  /(self: ulong, other: ulong): ulong
  {
    :::div(self, other)
  }

  %(self: ulong, other: ulong): ulong
  {
    :::mod(self, other)
  }

  &(self: ulong, other: ulong): ulong
  {
    :::and(self, other)
  }

  |(self: ulong, other: ulong): ulong
  {
    :::or(self, other)
  }

  ^(self: ulong, other: ulong): ulong
  {
    :::xor(self, other)
  }

  <<(self: ulong, other: ulong): ulong
  {
    :::shl(self, other)
  }

  >>(self: ulong, other: ulong): ulong
  {
    :::shr(self, other)
  }

  ==(self: ulong, other: ulong): bool
  {
    :::eq(self, other)
  }

  !=(self: ulong, other: ulong): bool
  {
    :::ne(self, other)
  }

  <(self: ulong, other: ulong): bool
  {
    :::lt(self, other)
  }

  <=(self: ulong, other: ulong): bool
  {
    :::le(self, other)
  }

  >(self: ulong, other: ulong): bool
  {
    :::gt(self, other)
  }

  >=(self: ulong, other: ulong): bool
  {
    :::ge(self, other)
  }

  min(self: ulong, other: ulong): ulong
  {
    :::min(self, other)
  }

  max(self: ulong, other: ulong): ulong
  {
    :::max(self, other)
  }

  -(self: ulong): ulong
  {
    :::neg(self)
  }

  !(self: ulong): bool
  {
    :::not(self)
  }

  bool(self: ulong): bool
  {
    self != 0.ulong
  }

  i8(self: ulong): i8
  {
    :::convi8(self)
  }

  i16(self: ulong): i16
  {
    :::convi16(self)
  }

  i32(self: ulong): i32
  {
    :::convi32(self)
  }

  i64(self: ulong): i64
  {
    :::convi64(self)
  }

  u8(self: ulong): u8
  {
    :::convu8(self)
  }

  u16(self: ulong): u16
  {
    :::convu16(self)
  }

  u32(self: ulong): u32
  {
    :::convu32(self)
  }

  u64(self: ulong): u64
  {
    :::convu64(self)
  }

  ilong(self: ulong): ilong
  {
    :::convilong(self)
  }

  ulong(self: ulong): ulong
  {
    self
  }

  isize(self: ulong): isize
  {
    :::convisize(self)
  }

  usize(self: ulong): usize
  {
    :::convusize(self)
  }

  f32(self: ulong): f32
  {
    :::convf32(self)
  }

  f64(self: ulong): f64
  {
    :::convf64(self)
  }
}
