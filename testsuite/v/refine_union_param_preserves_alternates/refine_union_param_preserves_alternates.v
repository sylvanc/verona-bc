// Regression test: refine_function_params used to replace a generic param's
// declared union type with the call site's argument type when the current
// param type matched the seed (reified def type). For Union(_node, none),
// passing `none` as the actual would replace the param type with just
// `none` — losing the _node alternate. Then the class field derived from
// the create param inherited the wrong type, and matches against the field
// failed at runtime with "bad type".
//
// The fix: don't replace a Union with the actual arg unless the Union has
// genuinely unresolved members (TypeId for shapes, or Dyn). Fully-concrete
// Unions are valid declared types and should be merged with — not replaced
// by — call-site arguments.

list[T]
{
  _node
  {
    value: T;
    next: _node | none;
    prev: _node | none;

    create(value: T, next: _node | none, prev: _node | none): _node
    {
      new { value, next, prev }
    }
  }

  _head: _node | none;
  _tail: _node | none;

  create(): list[T] { new { _head = none, _tail = none } }

  push_back(self: list[T], v: T): list[T]
  {
    // Constructs a node with two _node | none args. One is the literal
    // `none` (which used to wrongly narrow the next param to `none`).
    let n = _node(v, none, self._tail);
    self._tail = n;
    match self._head { (h: list[T]::_node) -> none } else { self._head = n };
    self
  }

  each(self: list[T], f: T -> none): none
  {
    var cur = self._head;
    while true
    {
      let val = match cur
      {
        // This typetest used to fail at runtime because cur's static type
        // was Union(_node, none) but the field's reified type was just
        // `none`, so loading the value through the field gave a value
        // tagged as none even though the runtime contained a _node pointer.
        (n: list[T]::_node) -> { cur = n.next; n.value }
      }
      else { return }
      f val
    }
  }
}

main(): none
{
  let l = list[i32]();
  l.push_back 1;
  l.push_back 2;
  // Walk the list manually; the bug used to make this match fail at
  // runtime with "bad type" because `_head` was typed as `none` due to
  // the create param being narrowed.
  var cur = l._head;
  match cur
  {
    (n: list[i32]::_node) -> none
  }
  else
  {
    // If the bug were still present we'd hit this branch incorrectly,
    // but the match would have already faulted.
    none
  }
}
