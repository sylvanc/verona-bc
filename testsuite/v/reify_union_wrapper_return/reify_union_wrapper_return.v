_lock
{
  _batching: bool;

  once create(): cown[_lock]
  {
    cown(new { _batching = false })
  }

  run[A, B](some: A, handler: B->none): none
  {
    when (_lock, some._c) (l, c) ->
    {
      (*l).acquire;
      handler(*c)
    }
  }

  acquire(self: _lock): none {}
}

shape read_cb
{
  apply(self: self, data: array[u8], size: usize): none;
}

signal
{
  _state
  {
    start(self: _state, h: ()->none): none
    {
      h()
    }

    close(self: _state): none {}
  }

  _c: cown[_state];

  create(): signal
  {
    new { _c = cown _state }
  }

  start(self: signal, h: ()->none): none
  {
    self _lock::run s -> s.start h;
  }

  close(self: signal): none
  {
    self _lock::run s -> s.close;
  }
}

file
{
  _state
  {
    start(self: _state, h: read_cb): none
    {
      h(array[u8]::fill(1), 1)
    }

    close(self: _state): none {}
  }

  _c: cown[_state];

  create(): file
  {
    new { _c = cown _state }
  }

  start(self: file, h: read_cb): none
  {
    self _lock::run f -> f.start h;
  }

  close(self: file): none
  {
    self _lock::run f -> f.close;
  }
}

pipe
{
  _state
  {
    start(self: _state, h: read_cb): none
    {
      h(array[u8]::fill(1), 1)
    }

    close(self: _state): none {}
  }

  _c: cown[_state];

  create(): pipe
  {
    new { _c = cown _state }
  }

  start(self: pipe, h: read_cb): none
  {
    self _lock::run p -> p.start h;
  }

  close(self: pipe): none
  {
    self _lock::run p -> p.close;
  }
}

tty
{
  _state
  {
    start(self: _state, h: read_cb): none
    {
      h(array[u8]::fill(1), 1)
    }

    close(self: _state): none {}
  }

  _c: cown[_state];

  create(): tty
  {
    new { _c = cown _state }
  }

  start(self: tty, h: read_cb): none
  {
    self _lock::run t -> t.start h;
  }

  close(self: tty): none
  {
    self _lock::run t -> t.close;
  }
}

stdin
{
  _impl: tty | pipe | file | none;

  create(impl: tty | pipe | file | none): stdin
  {
    new { _impl = impl }
  }

  start(self: stdin, on_read: read_cb): none
  {
    match self._impl
    {
      (impl: none) -> none;
      (impl: tty | pipe | file) -> impl.start on_read;
    }
  }

  close(self: stdin): none
  {
    match self._impl
    {
      (impl: none) -> none;
      (impl: tty | pipe | file) -> impl.close;
    }
  }
}

main(): i32
{
  let sig = signal;
  let t = tty;
  let p = pipe;
  let f = file;
  let empty = stdin(none);
  let in_tty = stdin(t);
  let in_pipe = stdin(p);
  let in_file = stdin(f);

  sig.start {};
  sig.close;

  empty.start (data, size) -> {};
  empty.close;

  in_tty.start (data, size) -> {};
  in_tty.close;

  in_pipe.start (data, size) -> {};
  in_pipe.close;

  in_file.start (data, size) -> {};
  in_file.close;

  0
}
