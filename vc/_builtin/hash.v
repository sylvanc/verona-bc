// Hashing protocol. Any type with a hash(self): u64 method satisfies
// the to_hash shape and can be used as a hash table key.
shape to_hash
{
  hash(self: self): u64;
}

hash(a: to_hash): u64
{
  a.hash
}
