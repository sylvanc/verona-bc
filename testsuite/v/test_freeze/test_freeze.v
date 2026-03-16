leaf
{
  val: i32;

  create(val: i32): leaf
  {
    new {val}
  }
}

node
{
  left: leaf;
  right: leaf;
  val: i32;

  create(left: leaf, right: leaf, val: i32): node
  {
    new {left, right, val}
  }
}

main(): i32
{
  var result = 0;

  // Test simple freeze and field load.
  var a = leaf(10);
  freeze(a);
  if a.val != 10 { result = result + 1; }

  // Test double freeze (no-op).
  var b = leaf(55);
  freeze(b);
  freeze(b);
  if b.val != 55 { result = result + 2; }

  // Test deeper topology: node -> two leaves.
  // Freeze the root, then load fields through the frozen graph.
  var l = leaf(3);
  var r = leaf(7);
  var n = node(l, r, 100);
  freeze(n);
  if n.val != 100 { result = result + 4; }
  if n.left.val != 3 { result = result + 8; }
  if n.right.val != 7 { result = result + 16; }

  result
}
