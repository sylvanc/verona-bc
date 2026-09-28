# 28. Initialization and Teardown

[← Table of Contents](README.md) | [Previous: Common Patterns](27-common-patterns.md)

Verona programs can initialize process-lifetime values through source `once`
functions, FFI library initializers, and compiler-generated singleton memos.
FFI initializers may also return callbacks that run during scheduler-aware
teardown. These mechanisms share memo storage, but they have different sources
and ordering rules.

This chapter identifies the initialization units and defines the initialization
dependency graph, including reflexive FFI dependencies. It then explains the
`AtTeardown` edges that extend dependencies through teardown callbacks and the
runtime sequence that executes both phases.

A **quiescent phase** is a scheduler point at which currently runnable work has
drained and work registered for quiescence may run.

---

## 28.1 Lifecycle Overview

Initialization completes before `main` starts. The compiler emits reachable
`once` memo declarations in dependency-first order. The runtime creates the
scheduler first, initializes memo slots in that emitted declaration order, and
then starts `main`. Source and compiler-generated FFI once initializers share
this order.

Teardown runs after `main` and all currently scheduled behaviors become
quiescent. An FFI initializer can return a callable value. The compiler emits an
`AtTeardown` operation that registers this value as scheduler work. Registered
callbacks run in last-in, first-out order, one at each subsequent quiescence, so
a callback may schedule more Verona work before teardown continues.

Once values and their reachable object graphs have process lifetime. They
remain available while teardown callbacks run. The runtime clears the memo slot
handles only after scheduler execution completes; the immortalized object
graphs remain valid for the rest of the process.

---

## 28.2 Initialization Units

A **once initializer** is the body that computes one memoized process-lifetime
value. Source `once` functions and FFI initializers both become once
initializers before **reification**, the compiler stage that specializes
reachable generic code and constructs the final function dependency graph.

### Source `once`

A source declaration such as:

```verona
once configuration(): config
{
  config::create()
}
```

is reified as two functions and one memo slot. The public function loads the
slot. The initializer contains the original body. The emitted memo declaration
associates the slot with that initializer.

### FFI Initializers

An `init` function inside an FFI `use` block is lowered to a hidden source
`once` function. The initializer may compute Verona state without calling its
own native library. In the usual library-lifecycle case, however, initialization
calls a native setup operation. Calls to the initializer's own library are
allowed, as described in the next section.

An FFI `init` must be non-`ref` and parameterless because startup invokes its
hidden memo initializer without arguments.

The hidden function returns `none`. Before returning, it registers the source
return value with `AtTeardown`. A returned callable therefore becomes a
teardown callback; `none` registers no work. The callable must expose exactly
one eligible non-generic, non-`ref` `apply` method that accepts only `self`.
When reification resolves that method, the compiler registers it as
`@callback` and records the `AtTeardown` dependency edge. The teardown path
does not currently diagnose failure to resolve the callback class or an
eligible method; in that case it emits neither registration nor dependency
edge, and runtime lookup fails. Invalid non-`none` values and wrong-arity
methods are likewise not rejected statically and fail at runtime.

### Singleton Memos

The compiler also creates memo slots for empty-class singleton values. These
slots use the same runtime memo storage and acquire process lifetime. They are
not source or FFI once initializers, so they are not roots in the once
dependency analysis. Their initializers only construct the singleton instance,
and the runtime executes them eagerly in memo declaration order.

---

## 28.3 FFI Initialization Dependencies

Each reachable FFI operation adds a compiler dependency on its library's hidden
once initializer. The emitted FFI operation remains a direct native call.

```verona
network_setup
{
  run(): none
  {
    :::setup();
  }
}

network_shutdown
{
  run(): none
  {
    :::shutdown();
  }
}

use "network"
{
  init(): any
  {
    network_setup::run();
    { network_shutdown::run(); }
  }

  setup = "network_setup"(): none;
  shutdown = "network_shutdown"(): none;
  send = "network_send"(array[u8]): none;
}
```

Reachable calls to `send`, `shutdown`, and `setup` establish dependencies on
`"network"`. The call to `setup` is reached while the `"network"` initializer
is already active, so that edge is reflexive and the compiler ignores it. The
same rule applies when an initializer or teardown callback reaches an operation
on its own library through ordinary Verona helpers.

A call from one library to another library is not reflexive. It creates an
ordinary dependency, so the other library initializes first. A cycle involving
two or more initialization units is rejected because none can initialize first.
See [FFI §17.1](17-ffi.md) for declaration syntax and [FFI §17.6](17-ffi.md)
for initializer behavior.

---

## 28.4 Initialization Dependency Graph

Reification builds a function dependency graph whose vertices are reified
functions. Each edge retains its source site for diagnostics and to identify
the edge kind. Static calls add edges immediately. Dynamic calls add edges
after shape resolution has identified the possible receiver classes. An FFI
operation adds an ordering-only edge from its enclosing function to the hidden
once initializer for its library. `AtTeardown` adds an edge from the
registering function to the callback's registered `@callback` method.

Every reachable once initializer is a root in the ordering traversal. An edge
A → B means that A depends on B. The compiler emits B's memo before A's memo,
so graph edges point from a dependent to its dependency. The traversal follows
the edge from A to B, then emits B when returning from that traversal before it
emits A. This postorder gives the runtime a dependency-first initialization
order.

The traversal permits recursive cycles containing only ordinary functions. An
FFI edge may return to the currently active FFI initializer, because calling an
operation on a library does not require that same initializer to run again. An
`AtTeardown` path may likewise return to the once initializer that owns that
teardown lifetime. The traversal rejects other cycles containing a once
initializer, because no initializer in such a cycle can be evaluated first.
The diagnostic reports the source edge sites around the cycle.

---

## 28.5 Teardown Lifetime Dependencies

A **teardown callback** is the callable returned by an FFI initializer and
registered by `AtTeardown`. A teardown callback is reachable code, but
registration does not execute it synchronously. Its `AtTeardown` edge is
therefore distinguished from an ordinary call edge even though both occupy the
same function dependency graph. The callback's own static and dynamic calls
continue the dependency path.

The callback may use another once value during teardown. That use imposes a
**lifetime dependency**. If initializer A registers a teardown callback that
can reach once initializer B, the graph contains a path from A through the
`AtTeardown` edge to B. The ordering traversal therefore emits B before A:

```text
initialize B
register B teardown
initialize A
register A teardown
```

Last-in, first-out teardown reverses that order:

```text
run A teardown
run B teardown
```

B therefore remains initialized throughout A's callback.

Every `AtTeardown` edge originates in the hidden once initializer that performs
the registration, so the registered callback belongs to that initializer's
teardown lifetime. Permission to revisit that initializer follows the path from
the edge. Entering another once initializer clears the permission, and that
initializer's dependencies are checked normally.

A callback may use the same once value whose initializer registered it. This
self-reference is permitted because that value is already live until teardown
finishes. Reaching another once initializer ends that permission; dependencies
from the reached initializer are checked normally. In contrast, mutual
lifetime dependencies are invalid:

```text
A teardown uses B
B teardown uses A
```

No last-in, first-out order can keep both dependencies alive. The ordinary once
cycle check rejects this program.

---

## 28.6 Worked Ordering Example

Suppose library A registers a callback that calls an ordinary helper. The
helper invokes an operation from library B:

```verona
library_b
{
  use_it(): none
  {
    :::operation_b();
  }

  use "b"
  {
    init(): any
    {
      :::start_b();
      { :::stop_b(); }
    }

    start_b = "start_b"(): none;
    stop_b = "stop_b"(): none;
    operation_b = "operation_b"(): none;
  }
}

library_a
{
  finish(): none
  {
    library_b::use_it();
  }

  start(): none
  {
    :::operation_a();
  }

  use "a"
  {
    init(): any
    {
      :::start_a();
      { library_a::finish(); }
    }

    start_a = "start_a"(): none;
    operation_a = "operation_a"(): none;
  }
}

main(): none
{
  library_a::start();
}
```

`main` calls `library_a::start`, whose `operation_a` makes A reachable. A's
callback reaches B's operation through `library_b::use_it`, so the compiler
finds a dependency path from A to B. B initializes first, but A tears down
first.

The helper does not need an annotation. Its ordinary call edges continue the
dependency path from A's `AtTeardown` edge to B.

---

## 28.7 Runtime Sequence

The compiler and runtime divide responsibility as follows:

1. `ffiinit` lowers FFI initializers. It rejects duplicate initializers for one
   library in an enclosing class.
2. `reify` constructs static, dynamic, FFI, and `AtTeardown` dependency edges
   and registers callback methods.
3. Still within `reify`, the once lowering traverses the completed graph,
   rejects invalid cycles, and emits memo declarations in dependency-first
   order.
4. VBCI initializes the scheduler and allocates all memo slots.
5. VBCI runs memo initializers in the dependency-first declaration order
   emitted by the compiler. `MemoSlot` reads an already initialized slot; a VIR
   initializer may load only memo slots declared earlier.
6. `AtTeardown` wraps each callable in scheduler work and schedules it for
   quiescence.
7. After normal work becomes quiescent, the scheduler runs the most recently
   registered teardown item. Teardown continues across later quiescent points
   until no registered callbacks or scheduled behaviors remain.
8. VBCI clears memo slots only after scheduler execution has completed.

This division keeps source syntax independent of runtime scheduling while
preserving the ordering needed by process-lifetime values and external
libraries.
