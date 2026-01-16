usize
{
  create(some: u64): usize
  {
    :::convusize(some)
  }

  +(self: usize, other: usize): usize
  {
    :::add(self, other)
  }

  -(self: usize, other: usize): usize
  {
    :::sub(self, other)
  }

  *(self: usize, other: usize): usize
  {
    :::mul(self, other)
  }

  /(self: usize, other: usize): usize
  {
    :::div(self, other)
  }

  %(self: usize, other: usize): usize
  {
    :::mod(self, other)
  }

  &(self: usize, other: usize): usize
  {
    :::and(self, other)
  }

  |(self: usize, other: usize): usize
  {
    :::or(self, other)
  }

  ^(self: usize, other: usize): usize
  {
    :::xor(self, other)
  }

  <<(self: usize, other: usize): usize
  {
    :::shl(self, other)
  }

  >>(self: usize, other: usize): usize
  {
    :::shr(self, other)
  }

  ==(self: usize, other: usize): bool
  {
    :::eq(self, other)
  }

  !=(self: usize, other: usize): bool
  {
    :::ne(self, other)
  }

  <(self: usize, other: usize): bool
  {
    :::lt(self, other)
  }

  <=(self: usize, other: usize): bool
  {
    :::le(self, other)
  }

  >(self: usize, other: usize): bool
  {
    :::gt(self, other)
  }

  >=(self: usize, other: usize): bool
  {
    :::ge(self, other)
  }

  min(self: usize, other: usize): usize
  {
    :::min(self, other)
  }

  max(self: usize, other: usize): usize
  {
    :::max(self, other)
  }

  -(self: usize): usize
  {
    :::neg(self)
  }

  !(self: usize): bool
  {
    :::not(self)
  }

  bool(self: usize): bool
  {
    self != 0.usize
  }

  i8(self: usize): i8
  {
    :::convi8(self)
  }

  i16(self: usize): i16
  {
    :::convi16(self)
  }

  i32(self: usize): i32
  {
    :::convi32(self)
  }

  i64(self: usize): i64
  {
    :::convi64(self)
  }

  u8(self: usize): u8
  {
    :::convu8(self)
  }

  u16(self: usize): u16
  {
    :::convu16(self)
  }

  u32(self: usize): u32
  {
    :::convu32(self)
  }

  u64(self: usize): u64
  {
    :::convu64(self)
  }

  ilong(self: usize): ilong
  {
    :::convilong(self)
  }

  ulong(self: usize): ulong
  {
    :::convulong(self)
  }

  isize(self: usize): isize
  {
    :::convisize(self)
  }

  usize(self: usize): usize
  {
    self
  }

  f32(self: usize): f32
  {
    :::convf32(self)
  }

  f64(self: usize): f64
  {
    :::convf64(self)
  }
}
