ilong
{
  create(some: ilong = 0): ilong
  {
    some
  }

  +(self: ilong, other: ilong): ilong
  {
    :::add(self, other)
  }

  -(self: ilong, other: ilong): ilong
  {
    :::sub(self, other)
  }

  *(self: ilong, other: ilong): ilong
  {
    :::mul(self, other)
  }

  /(self: ilong, other: ilong): ilong
  {
    :::div(self, other)
  }

  %(self: ilong, other: ilong): ilong
  {
    :::mod(self, other)
  }

  &(self: ilong, other: ilong): ilong
  {
    :::and(self, other)
  }

  |(self: ilong, other: ilong): ilong
  {
    :::or(self, other)
  }

  ^(self: ilong, other: ilong): ilong
  {
    :::xor(self, other)
  }

  <<(self: ilong, other: ilong): ilong
  {
    :::shl(self, other)
  }

  >>(self: ilong, other: ilong): ilong
  {
    :::shr(self, other)
  }

  ==(self: ilong, other: ilong): bool
  {
    :::eq(self, other)
  }

  !=(self: ilong, other: ilong): bool
  {
    :::ne(self, other)
  }

  <(self: ilong, other: ilong): bool
  {
    :::lt(self, other)
  }

  <=(self: ilong, other: ilong): bool
  {
    :::le(self, other)
  }

  >(self: ilong, other: ilong): bool
  {
    :::gt(self, other)
  }

  >=(self: ilong, other: ilong): bool
  {
    :::ge(self, other)
  }

  min(self: ilong, other: ilong): ilong
  {
    :::min(self, other)
  }

  max(self: ilong, other: ilong): ilong
  {
    :::max(self, other)
  }

  -(self: ilong): ilong
  {
    :::neg(self)
  }

  !(self: ilong): ilong
  {
    :::not(self)
  }

  abs(self: ilong): ilong
  {
    :::abs(self)
  }
  
  bool(self: ilong): bool
  {
    self != 0.ilong
  }

  i8(self: ilong): i8
  {
    :::convi8(self)
  }

  i16(self: ilong): i16
  {
    :::convi16(self)
  }

  i32(self: ilong): i32
  {
    :::convi32(self)
  }

  i64(self: ilong): i64
  {
    :::convi64(self)
  }

  u8(self: ilong): u8
  {
    :::convu8(self)
  }

  u16(self: ilong): u16
  {
    :::convu16(self)
  }

  u32(self: ilong): u32
  {
    :::convu32(self)
  }

  u64(self: ilong): u64
  {
    :::convu64(self)
  }

  ilong(self: ilong): ilong
  {
    :::convilong(self)
  }

  ulong(self: ilong): ulong
  {
    :::convulong(self)
  }

  isize(self: ilong): isize
  {
    self
  }

  usize(self: ilong): usize
  {
    :::convusize(self)
  }

  f32(self: ilong): f32
  {
    :::convf32(self)
  }

  f64(self: ilong): f64
  {
    :::convf64(self)
  }
}
