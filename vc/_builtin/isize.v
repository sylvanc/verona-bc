isize
{
  create(some: u64): isize
  {
    :::convisize(some)
  }

  +(self: isize, other: isize): isize
  {
    :::add(self, other)
  }

  -(self: isize, other: isize): isize
  {
    :::sub(self, other)
  }

  *(self: isize, other: isize): isize
  {
    :::mul(self, other)
  }

  /(self: isize, other: isize): isize
  {
    :::div(self, other)
  }

  %(self: isize, other: isize): isize
  {
    :::mod(self, other)
  }

  &(self: isize, other: isize): isize
  {
    :::and(self, other)
  }

  |(self: isize, other: isize): isize
  {
    :::or(self, other)
  }

  ^(self: isize, other: isize): isize
  {
    :::xor(self, other)
  }

  <<(self: isize, other: isize): isize
  {
    :::shl(self, other)
  }

  >>(self: isize, other: isize): isize
  {
    :::shr(self, other)
  }

  ==(self: isize, other: isize): bool
  {
    :::eq(self, other)
  }

  !=(self: isize, other: isize): bool
  {
    :::ne(self, other)
  }

  <(self: isize, other: isize): bool
  {
    :::lt(self, other)
  }

  <=(self: isize, other: isize): bool
  {
    :::le(self, other)
  }

  >(self: isize, other: isize): bool
  {
    :::gt(self, other)
  }

  >=(self: isize, other: isize): bool
  {
    :::ge(self, other)
  }

  min(self: isize, other: isize): isize
  {
    :::min(self, other)
  }

  max(self: isize, other: isize): isize
  {
    :::max(self, other)
  }

  -(self: isize): isize
  {
    :::neg(self)
  }

  !(self: isize): bool
  {
    :::not(self)
  }

  abs(self: isize): isize
  {
    :::abs(self)
  }

  bool(self: isize): bool
  {
    self != 0.isize
  }

  i8(self: isize): i8
  {
    :::convi8(self)
  }

  i16(self: isize): i16
  {
    :::convi16(self)
  }

  i32(self: isize): i32
  {
    :::convi32(self)
  }

  i64(self: isize): i64
  {
    :::convi64(self)
  }

  u8(self: isize): u8
  {
    :::convu8(self)
  }

  u16(self: isize): u16
  {
    :::convu16(self)
  }

  u32(self: isize): u32
  {
    :::convu32(self)
  }

  u64(self: isize): u64
  {
    :::convu64(self)
  }

  ilong(self: isize): ilong
  {
    :::convilong(self)
  }

  ulong(self: isize): ulong
  {
    :::convulong(self)
  }

  isize(self: isize): isize
  {
    self
  }

  usize(self: isize): usize
  {
    :::convusize(self)
  }

  f32(self: isize): f32
  {
    :::convf32(self)
  }

  f64(self: isize): f64
  {
    :::convf64(self)
  }
}
