f64
{
  create(some: f64): f64
  {
    some
  }

  +(self: f64, other: f64): f64
  {
    :::add(self, other)
  }

  -(self: f64, other: f64): f64
  {
    :::sub(self, other)
  }

  *(self: f64, other: f64): f64
  {
    :::mul(self, other)
  }

  /(self: f64, other: f64): f64
  {
    :::div(self, other)
  }

  %(self: f64, other: f64): f64
  {
    :::mod(self, other)
  }

  **(self: f64, other: f64): f64
  {
    :::pow(self, other)
  }

  ==(self: f64, other: f64): bool
  {
    :::eq(self, other)
  }

  !=(self: f64, other: f64): bool
  {
    :::ne(self, other)
  }

  <(self: f64, other: f64): bool
  {
    :::lt(self, other)
  }

  <=(self: f64, other: f64): bool
  {
    :::le(self, other)
  }

  >(self: f64, other: f64): bool
  {
    :::gt(self, other)
  }

  >=(self: f64, other: f64): bool
  {
    :::ge(self, other)
  }

  min(self: f64, other: f64): f64
  {
    :::min(self, other)
  }

  max(self: f64, other: f64): f64
  {
    :::max(self, other)
  }

  logbase(self: f64, other: f64): f64
  {
    :::logbase(self, other)
  }

  atan2(self: f64, other: f64): f64
  {
    :::atan2(self, other)
  }

  -(self: f64): f64
  {
    :::neg(self)
  }

  abs(self: f64): f64
  {
    :::abs(self)
  }

  ceil(self: f64): f64
  {
    :::ceil(self)
  }

  floor(self: f64): f64
  {
    :::floor(self)
  }

  exp(self: f64): f64
  {
    :::exp(self)
  }

  log(self: f64): f64
  {
    :::log(self)
  }

  sqrt(self: f64): f64
  {
    :::sqrt(self)
  }

  cbrt(self: f64): f64
  {
    :::cbrt(self)
  }

  isinf(self: f64): bool
  {
    :::isinf(self)
  }

  isnan(self: f64): bool
  {
    :::isnan(self)
  }

  sin(self: f64): f64
  {
    :::sin(self)
  }

  cos(self: f64): f64
  {
    :::cos(self)
  }

  tan(self: f64): f64
  {
    :::tan(self)
  }

  asin(self: f64): f64
  {
    :::asin(self)
  }

  acos(self: f64): f64
  {
    :::acos(self)
  }

  atan(self: f64): f64
  {
    :::atan(self)
  }

  sinh(self: f64): f64
  {
    :::sinh(self)
  }

  cosh(self: f64): f64
  {
    :::cosh(self)
  }

  tanh(self: f64): f64
  {
    :::tanh(self)
  }

  asinh(self: f64): f64
  {
    :::asinh(self)
  }

  acosh(self: f64): f64
  {
    :::acosh(self)
  }

  atanh(self: f64): f64
  {
    :::atanh(self)
  }

  e(): f64
  {
    :::e()
  }

  pi(): f64
  {
    :::pi()
  }

  inf(): f64
  {
    :::inf()
  }

  nan(): f64
  {
    :::nan()
  }

  bool(self: f64): bool
  {
    self != 0.f64
  }

  i8(self: f64): i8
  {
    :::convi8(self)
  }

  i16(self: f64): i16
  {
    :::convi16(self)
  }

  i32(self: f64): i32
  {
    :::convi32(self)
  }

  i64(self: f64): i64
  {
    :::convi64(self)
  }

  u8(self: f64): u8
  {
    :::convu8(self)
  }

  u16(self: f64): u16
  {
    :::convu16(self)
  }

  u32(self: f64): u32
  {
    :::convu32(self)
  }

  u64(self: f64): u64
  {
    :::convu64(self)
  }

  ilong(self: f64): ilong
  {
    :::convilong(self)
  }

  ulong(self: f64): ulong
  {
    :::convulong(self)
  }

  isize(self: f64): isize
  {
    :::convisize(self)
  }

  usize(self: f64): usize
  {
    :::convusize(self)
  }

  f32(self: f64): f32
  {
    :::convf32(self)
  }

  f64(self: f64): f64
  {
    self
  }
}
