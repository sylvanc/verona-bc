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
}
