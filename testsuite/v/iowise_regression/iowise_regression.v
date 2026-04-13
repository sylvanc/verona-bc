// Minimal self-contained regression for the iowise infer/reify failures.
use to_string = string | ()->string;

log
{
  info
  {
    value(self: info): i32
    {
      1
    }
  }

  use level = info;

  string(log_level: level): string
  {
    "[info] "
  }
}

async_writer[A]
{
  c: cown[A];
  terminal: bool;

  create(w: A): async_writer[A]
  {
    let t = w.terminal;
    new {c = cown w, terminal = t}
  }

  log(self: async_writer, log_level: log::level, msg: to_string): none
  {
    when self.c w ->
    {
      if log_level.value <= (*w).log_level.value
      {
        (*w).print(log::string log_level);

        match msg
        {
          (s: string) -> (*w).print(s);
          (s: ()->string) -> (*w).print(s());
        }
      }
    }
  }
}

out
{
  create(): out
  {
    new {}
  }
}

err
{
  create(): err
  {
    new {}
  }
}

_fd_writer
{
  fd: i32;
  log_level: log::level;

  create(capability: out | err): async_writer[_fd_writer]
  {
    let fd = match capability
    {
      (cap: out) -> 1;
      (cap: err) -> 2;
    }
    else
    {
      0
    }

    async_writer(new {fd, log_level = log::info})
  }

  print(self: _fd_writer, s: string): none
  {
  }

  terminal(self: _fd_writer): bool
  {
    false
  }
}

main(): none
{
  var result = 0;

  let stdout = _fd_writer(out());
  let stderr = _fd_writer(err());
  stdout.log(log::info, "hello");

  if !stdout.terminal
  {
    result = result + 1;
  }

  if !stderr.terminal
  {
    result = result + 2;
  }

  ffi::exit_code result
}
