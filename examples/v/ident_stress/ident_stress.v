// =============================================================================
// Ident pass stress tests
// =============================================================================
// Each section exercises a different feature of name resolution via TypeParent.
// Sections marked [BUG] trigger crashes or incorrect behavior.

// --- 1. Original resolve.v: nesting + alias + TypeParent ---
class1
{
  class4 {}
}

class2[T]
{
  use alias1 = T;
}

scope
{
  use class2[class1];

  class3
  {
    use alias1;

    // class4 resolves through alias1 → T → class1 → class4.
    // Expected: (typeparent)^3 :: class1 :: class4
    f(): class4 {}
  }
}

// --- 2. Shadowing: inner definition shadows outer ---
shadow_outer
{
  A
  {
    inner {}
  }

  B
  {
    // B::A shadows shadow_outer::A
    A
    {
      inner2 {}
    }

    // Should resolve to B::A, not shadow_outer::A.
    f(): A {}

    // Should resolve to B::A::inner2.
    g(): A::inner2 {}
  }
}

// --- 3. Type alias chains ---
// step2 → step1 → target, each at the same nesting level.
alias_chain
{
  target {}

  use step1 = target;
  use step2 = step1;

  // step2 resolves through two alias hops.
  f(): step2 {}
}

// --- 4. Generic type alias with substitution ---
generic_alias
{
  box[T]
  {
    val: T;
  }

  leaf {}

  // LeafBox = box[leaf]: alias with generic type arg.
  use LeafBox = box[leaf];

  f(): LeafBox {}
}

// --- 5. Nested generic alias through type parameter ---
nested_generic[T]
{
  wrapper[U]
  {
    use inner_alias = U;
  }

  use wrapper[T];

  class_using_alias
  {
    // We don't currently allow using a type parameter. This resolves to
    // `use T`.
    // use inner_alias;
    f(): inner_alias {}
  }
}

// --- 6. Multiple imports from the same module ---
multi_import
{
  lib
  {
    A {}
    B {}
    C {}
  }

  use lib;

  // All resolve through the `use lib` import.
  f(): A {}
  g(): B {}
  h(): C {}
}

// --- 7. Use imports for lookup, not lookdown ---
base_7
{
  leaf_7 {}
}

mid_7
{
  use base_7;

  // leaf_7 is visible here via unqualified lookup through the use import.
  f7(): leaf_7 {}
}

// --- 8. Type parameter across nesting levels ---
typeparam_nesting[T]
{
  inner
  {
    // T is outer's type param, resolved via TypeParent.
    // Expected: (typeparent)^2 :: T
    f(): T {}
  }
}

// --- 9. Alias with type arguments that themselves need resolution ---
alias_with_resolved_args
{
  pair[A, B]
  {
    fst: A;
    snd: B;
  }

  myclass {}

  // Both type args to pair are resolved names.
  use MyPair = pair[myclass, myclass];

  f(): MyPair {}
}

// --- 10. Direct qualified name, deep nesting, no use ---
deep_qual
{
  L1
  {
    L2
    {
      L3 {}
    }
  }

  // 3-element direct qualified name.
  f(): L1::L2::L3 {}
}

// --- 11. Alias of alias of generic class ---
alias_of_alias_generic
{
  container[T]
  {
    elem: T;
  }

  leaf {}

  use step1 = container[leaf];
  use step2 = step1;

  // Two alias hops, final target is generic.
  f(): step2 {}
}

// --- 12. Same name at different nesting depths ---
// Ensure the correct one is chosen at each level.
same_name_depth
{
  X {}

  inner
  {
    X {}

    // Should resolve to inner::X, not same_name_depth::X.
    f(): X {}
  }

  // Should resolve to same_name_depth::X.
  g(): X {}
}

// --- 13. Type parameter shadowed by inner type parameter ---
typeparam_shadow[T]
{
  inner[T]
  {
    // This T is inner's own T, not outer's T.
    f(): T {}
  }
}

// --- 14. Use within a function (import inside function scope) ---
use_in_func
{
  helper
  {
    helper_inner
    {
      create(): helper_inner
      {
        new {}
      }
    }
  }

  f()
  {
    use helper;
    helper_inner;
  }
}
