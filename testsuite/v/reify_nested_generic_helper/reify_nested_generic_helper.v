// Regression test for the reify pass bake_typename fix
// (commit 36132ef2). Exercises a nested helper class inside a
// generic parent, instantiated multiple times with different K, V.
// Each parent instantiation must reify its own helper-class
// reification — and the auto-generated field accessor's return
// type must point at the right per-instantiation helper.
//
// Pre-fix: the field accessor `_n::1::ref::N` for parent::M always
// returned `Ref(node::0 | none)` regardless of M, because the
// existing-reifications fallback in get_reification picked the FIRST
// helper reification's K, V at make_id materialisation time.
//
// Post-fix: each `_n::1::ref::N` correctly returns
// `Ref(node::M' | none)` where M' is the helper reified with the
// matching parent's K, V. Match arms downstream see the correct
// types and the runtime values come back unmodified.

generic[K, V]
{
  node
  {
    key: K;
    val: V;
  }

  _n: node | none;

  create(): generic[K, V]
  {
    new { _n = none }
  }

  put(self: generic[K, V], k: K, v: V): none
  {
    self._n = node(k, v)
  }

  get_val(self: generic[K, V]): V | none
  {
    match self._n
    {
      (n: node) -> n.val
    }
    else { none }
  }

  // Lambda capturing K, V — lifted by sugar to a class with its own
  // [K, V] TypeParams. Inside, references `node` — exercises the
  // parent_tps_chain Function-ancestor inclusion in find_or_push.
  has_match(self: generic[K, V], k: K): bool
  {
    let lam = (n: node) -> { (n.key) == k };
    match self._n
    {
      (n: node) -> lam(n)
    }
    else { false }
  }
}

main(): none
{
  // Two different parent instantiations.
  let a = generic[i32, i32]::create();
  a.put(1, 100);

  let b = generic[u8, u64]::create();
  b.put(u8 2, u64 200);

  // Encode each check as a 0-or-N value via a returning match. The
  // var-result-with-side-effect pattern hits a separate latent
  // typing issue and isn't suitable here.
  let av: i32 = match a.get_val
  {
    (v: i32) -> if v == 100 { i32 0 } else { i32 1 }
  }
  else { i32 2 };

  let bv: i32 = match b.get_val
  {
    (v: u64) -> if v == u64 200 { i32 0 } else { i32 4 }
  }
  else { i32 8 };

  // Lambda-with-node-capture: must dispatch correctly per
  // instantiation. has_match should return true when the key
  // matches and false otherwise.
  let am: i32 = if a.has_match(1) { i32 0 } else { i32 16 };
  let am2: i32 = if a.has_match(99) { i32 64 } else { i32 0 };

  let bm: i32 = if b.has_match(u8 2) { i32 0 } else { i32 32 };
  let bm2: i32 = if b.has_match(u8 99) { i32 128 } else { i32 0 };

  let result: i32 = av + bv + am + am2 + bm + bm2;

  ffi::exit_code result
}
