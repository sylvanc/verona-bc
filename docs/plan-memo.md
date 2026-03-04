# Plan: `once` Functions (Memoized Zero-Arg)

## Summary

Functions marked `once` are memoized: evaluated eagerly at program startup
(before `main`) in dependency order, with the original function body replaced
by a stub that loads from a memo slot. All dispatch mechanisms (Call, CallDyn,
TryCallDyn, Tailcall, FnPointer) work transparently.

## Design Decisions (settled)

1. **Opt-in memoization via `once`**: Only functions explicitly marked `once`
   are memoized. Zero-arg functions without `once` call normally every time.
2. **Syntax**: `once` occupies the same grammar slot as `ref` in function
   declarations — `once f(): T { ... }`. Parsed as `T(Ident, "once")` in the
   structure pass, no new keyword needed.
3. **`once` + `ref` disallowed**: `once` and `ref` are mutually exclusive (same
   grammar slot).
4. **`once` requires zero params**: A `once` function with parameters is a
   compile error (enforced in the structure pass).
5. **`once` on methods prevented by zero-params check**: Verona methods
   explicitly list `self` in `Params` (e.g., `size(self: string): usize`), so
   they always have ≥1 parameter. The zero-params check in the structure pass
   is both necessary and sufficient to prevent `once` on methods.
6. **Body-rewrite, not call-site rewrite**: The memo pass splits each `FuncOnce`
   into (a) an init function with the original body and a new FunctionId, and
   (b) a stub function with the original FunctionId that loads from a slot.
   This means all callers — static, dynamic, tail, pointer — hit the stub
   transparently.
7. **`FuncOnce` IR node**: In the IR, `once` functions are emitted as
   `FuncOnce` (distinct from `Func`). The memo pass consumes all `FuncOnce`
   nodes and emits only `Func` + `MemoInit`. Downstream passes never see
   `FuncOnce`.
8. **Eager initialization**: `once` functions run before `main`, in dependency
   order determined by topological sort.
9. **Cycle detection at compile time**: Cycles among `once` functions (including
   through non-once intermediaries and CallDyn targets) are a compile error.
10. **Backend pass**: The memo pass lives in `vbcc`, applicable to any language
    targeting the IR.
11. **Return type safety (deferred)**: Originally planned to require `once`
    functions to return `cown`, but removed because the backend typecheck pass
    sees `when`-produced types as `dyn`, making the check unreliable. Will be
    revisited when the type system can track cown types through `when`.

## Known Limitations (documented)

- **Memo slot values are immortal**: Returned values are held for the program's
  lifetime and never collected. For the primary use case (cowns), this is
  desired.

---

## Part A: `once` Syntax and Frontend

### A1. Frontend — structure pass (`vc/passes/structure.cc`)

Change the function declaration pattern from:
```cpp
~T(Ident, "ref")[Lhs]
```
to:
```cpp
~(T(Ident, "ref") / T(Ident, "once"))[Lhs]
```

In the handler, change:
```cpp
// Before:
Node side = _(Lhs) ? Lhs : Rhs;

// After:
Node side;
if (!_(Lhs))
  side = Rhs;
else if (_(Lhs)->location() == "once")
  side = Once;
else
  side = Lhs;
```

Add validation:
- If `side == Once` and `Params` is non-empty: error "once functions must have
  no parameters".

### A2. Frontend — `Once` token and WF (`vc/lang.h`)

`Once` token is defined in `include/vbcc.h` (since `vc/lang.h` includes it).

Change `wfFuncLhs`:
```cpp
// Before:
inline const auto wfFuncLhs = Lhs >>= Lhs | Rhs;

// After:
inline const auto wfFuncLhs = Lhs >>= Lhs | Rhs;
inline const auto wfFuncDefLhs = Lhs >>= Lhs | Rhs | Once;
```

Use `wfFuncDefLhs` for `Function` definitions in both `wfPassStructure`
(line 170) and `wfPassANF` (line 316). Keep `wfFuncLhs` unchanged for
`Call`/`Lookup` nodes (lines 339, 340), which should never contain `Once`.

### A3. Frontend — handedness checks (`vc/lang.cc`, `vc/passes/infer.cc`)

`find_func_def()` in `lang.cc` filters overloads by handedness:
```cpp
if (hand && (d / Lhs)->type() != hand->type())
  continue;
```

A call to a `once` function has `hand == Rhs`, but `d / Lhs == Once`. Update:
```cpp
auto def_hand = (d / Lhs)->type();
if (hand && def_hand != hand->type() &&
    !(def_hand == Once && hand->type() == Rhs))
  continue;
```

Other hand-matching sites (~20 total across infer.cc, subtype.cc, sugar.cc,
reify.cc) are all safe: class method matching sites can't match `once`
functions (which have no `self`), and Lookup/Call node Lhs sites are
constrained by `wfFuncLhs` which excludes `Once`.

### A4. Frontend — reify pass (`vc/passes/reify.cc`)

When emitting a reified function, check `(def / Lhs) == Once`:
- If `Once`: emit `FuncOnce << FunctionId << Params << Type << Vars << Labels`
  where `Params` is an empty `Params` node.
- Otherwise: emit `Func << FunctionId << Params << Type << Vars << Labels`
  (as now).

`FuncOnce` has the same child structure as `Func` (including empty `Params`).
This avoids cascading changes to downstream passes — the memo pass consumes
`FuncOnce` before they run.

**Handedness**: `once` functions are always rhs (no `::ref` in FunctionId).
In `make_id()`, `Once` is not `Lhs`, so no suffix — correct by default.
In `reify_call`'s accept filter:
```cpp
// Before:
return (def == Function) && ((def / Params)->size() == arity) &&
  ((def / Lhs) == hand);

// After:
return (def == Function) && ((def / Params)->size() == arity) &&
  (((def / Lhs) == hand) || ((def / Lhs) == Once && hand == Rhs));
```

**Return type check**: After emitting `FuncOnce`, verify the return type is
`Cown`. If not, emit a compile error: "once functions must return cown".
This check happens post-reify where all types are concrete.

---

## Part B: Memo Pass and Backend

### B1. Tokens (`include/vbcc.h`)

Add token definitions:
- `Once` — marker token for the `Lhs` slot on frontend `Function` nodes.
- `FuncOnce` — top-level IR node for once functions (consumed by memo pass).
- `MemoInit` — top-level IR node: ordered list of `FunctionId` children.
- `MemoSlot` — statement node: load from memo slot.

Add WF definitions:
```
(FuncOnce <<= FunctionId * Params * (Type >>= wfType) * Vars * Labels)
(MemoInit <<= FunctionId++)
(MemoSlot <<= wfDst * FunctionId)
```

Add `MemoSlot` to `wfStatement`.

Add `FuncOnce` and `MemoInit` to `wfIR`:
```
Top <<= (Primitive | Class | Type | Func | FuncOnce | Lib | MemoInit)++
```

After the memo pass runs, `FuncOnce` is gone from the tree. Downstream passes
only see `Func`, `MemoInit`, and `MemoSlot`.

### B2. New pass: `memo` (`vbcc/passes/memo.cc`)

**Position**: Before `assignids`, after reify output.

**Pipeline order** (both `vc/main.cc` and `vbcc/main.cc`):
```
... → memo → assignids → validids → liveness → typecheck
```

Placing memo before assignids avoids having to assign IDs for newly-created
functions (stubs and init functions). `assignids` naturally handles all
`Func` nodes produced by memo.

**WF**: `PassDef p{"memo", wfIR, dir::once};`

**Algorithm** (`dir::once` with `post()` hook):

1. **Collect `FuncOnce` nodes**: Scan `Top` children for `FuncOnce`. Build:
   - `std::set<std::string> memo_ids` — set of FunctionId strings.
   - `std::unordered_map<std::string, Node> func_map` — FunctionId → node
     (both `Func` and `FuncOnce`, for body traversal during dependency
     discovery).

2. **Build dependency graph**: For each `FuncOnce`, DFS through its call graph
   to find all transitively-reachable `once` functions:

   - For each `Call` in the current function body, check the target FunctionId.
     If the target is in `memo_ids`, add a dependency edge.
     If the target is a regular `Func`, recurse into its body.

   - For each `CallDyn`/`TryCallDyn`, resolve the target(s) using the
     preceding `Lookup`'s source type and method ID — same pattern as
     typecheck.cc's `resolve_one`:
     - **Type environment**: To determine the Lookup source register's type,
       the memo pass builds a lightweight register→type map by scanning
       statements in order: `New`/`Stack` → ClassId, `Const` → primitive type,
       `Copy`/`Move` → propagate, `Call` → function return type. This doesn't
       need full type inference — only enough to resolve receiver types for
       Lookup statements that precede CallDyn.
     - Find the class/primitive for the source type.
     - Look up the MethodId in its Methods table → concrete FunctionId.
     - If in `memo_ids`, add edge. Otherwise recurse.
     - For `Union` source types, resolve each member.
     - For `Dyn` or unresolvable types, skip (documented limitation).

   - **Visited set**: Per-`FuncOnce` root, maintain a `std::set<std::string>`
     of already-explored non-memo function bodies. Reset between roots.
     Prevents infinite recursion through non-once mutual recursion.

3. **Topological sort + cycle detection**: Standard DFS-based toposort on the
   dependency graph (edges among `once` functions only). If a back edge is
   found, emit a compile error with the cycle path.

4. **Split each `FuncOnce` into stub + init**:

   For `FuncOnce "f"` with body B and return type T:

   **Init function** (new FunctionId, original body):
   ```
   Func << (FunctionId ^ "f$once") << Params << T << Vars << Labels(B)
   ```

   **Stub function** (original FunctionId, loads from slot):
   ```
   Func << (FunctionId ^ "f") << (Params) << T
        << (Vars << (LocalId ^ "$memo"))
        << (Labels << (Label << (LabelId ^ "entry")
                             << (Body << (MemoSlot << (LocalId ^ "$memo")
                                                   << (FunctionId ^ "f$once")))
                             << (Return << (LocalId ^ "$memo"))))
   ```

   Replace the `FuncOnce` node in `Top` with both `Func` nodes.

5. **Emit `MemoInit`**: Create `MemoInit` with `FunctionId` children in
   topological order (callees before callers). Use the init function IDs
   (`"f$once"`). Append to `Top`.

### B3. Backend Def pattern (`vbcc/lang.h`)

Add `MemoSlot` to the `Def` pattern (it defines a LocalId).

Declare the memo pass:
```cpp
PassDef memo();
```

Note: `memo()` takes no `std::shared_ptr<Bytecode>` parameter since it runs
before assignids and doesn't need bytecode state.

### B4. Liveness (`vbcc/passes/liveness.cc`)

`MemoSlot` defines `LocalId` and uses nothing. Add to the "def only" category.

### B5. Typecheck (`vbcc/passes/typecheck.cc`)

`MemoSlot` type: look up the init function by FunctionId via `find_func()`,
use its return type. Since `MemoSlot` references the init function
(`f$once`), and `find_func` searches `Func` children of `Top`, it finds
the init function naturally.

### B6. Bytecode encoding (`vbcc/bytecode.cc`)

**Header**: Encode the memo init list between the Types section and the Code
section. Specifically, in the encoder: after the Types encoding loop
(bytecode.cc) and before the Code size write. In the decoder (program.cc):
after complex types parsing and before the code section.

If no `MemoInit` node exists in `Top`, encode `uleb(0)`. Otherwise encode
`uleb(count)`, then `uleb(func_id)` per entry (numeric IDs from assignids).

**Encoder mapping**: Before encoding functions, iterate the `MemoInit` node
and build a `std::unordered_map<std::string, size_t>` from FunctionId string
→ 0-based dense index. Use this map when encoding `MemoSlot` statements.

**Code**: New opcode `MemoLoad`:
```
Op::MemoLoad  dst_reg(uleb)  memo_slot_index(uleb)
```

The stub and init functions are both regular `Func` nodes, encoded with the
standard function format. No special function encoding needed.

### B7. Op enum + debug name (`include/vbci.h`, `vbci/thread.cc`)

Add `MemoLoad` to the `Op` enum.

Add to the op name debug output:
```cpp
case Op::MemoLoad: return os << "MemoLoad";
```

No bytecode version bump (pre-release).

### B8. Runtime — memo slots (`vbci/program.h`, `vbci/program.cc`)

Add to `Program`:
```cpp
std::vector<Register> memo_slots;
std::vector<size_t> memo_func_ids;
```

In `load()`: parse the memo init list (after complex types, matching the
encoder's header position), populate `memo_func_ids`.

In `run()`: after init functions, before scheduler start:
```cpp
memo_slots.resize(memo_func_ids.size());
for (size_t i = 0; i < memo_func_ids.size(); i++)
{
  memo_slots[i] = Thread::run_callback(&functions.at(memo_func_ids[i]), 0);
}
```

Add accessor:
```cpp
Register& memo_slot(size_t index) { return memo_slots.at(index); }
```

### B9. Interpreter (`vbci/thread.cc`)

Add `Op::MemoLoad` handler:
```cpp
case Op::MemoLoad:
{
  process([](Register& dst, Constant<size_t> slot) INLINE {
    auto& val = Program::get().memo_slot(slot);
    dst = val.borrow();
  });
  break;
}
```

### B10. Build system (`vbcc/CMakeLists.txt`)

Add `passes/memo.cc` to the `libvbcc` static library sources.

### B11. Pipeline registration

**`vbcc/main.cc`**: Add `memo()` before `assignids`.
**`vc/main.cc`**: Add `vbcc::memo()` before `vbcc::assignids`.

**VIR text parser**: `vbcc/passes/statements.cc` and `vbcc/passes/labels.cc`
match `T(Func)`. Since `FuncOnce` is only generated by `vc`'s reify pass (not
hand-written VIR), these are not updated. If VIR text support for `FuncOnce`
is needed later, add `T(FuncOnce)` rules in both.

---

## Part C: Tests

1. **`memo_basic`**: `once` function returns a cown, called twice, result is
   the same cown.
2. **`memo_deps`**: `once f()` calls `once g()`. Verify both evaluate, correct
   ordering.
3. **`memo_cycle`**: `once f()` calls `once g()`, `g()` calls `f()`. Compile
   error.
4. **`memo_not_once`**: Regular zero-arg function is NOT memoized, returns
   fresh value each call.
5. **`memo_once_params`**: `once f(x: i32)` — compile error, once with params.
6. **`memo_once_method`**: `once f(self: myclass): cown[i32]` inside a class
   — compile error (once with params, since `self` counts as a parameter).

---

## Touchpoint Summary

| File | Change |
|------|--------|
| `vc/passes/structure.cc` | Parse `once` in function Lhs slot; validate zero-params + not-method |
| `vc/lang.h` | `wfFuncDefLhs` adds `Once`; `wfFunction` uses it |
| `vc/lang.cc` | `find_func_def()` handedness check updated for `Once` |
| `vc/passes/reify.cc` | Emit `FuncOnce` for `once` functions; handedness filter; return type check |
| `vc/main.cc` | Add `vbcc::memo()` to pipeline |
| `include/vbcc.h` | `Once`, `FuncOnce`, `MemoInit`, `MemoSlot` tokens; WF updates |
| `include/vbci.h` | `MemoLoad` op |
| `vbcc/lang.h` | `MemoSlot` in `Def`; `memo()` pass declaration |
| `vbcc/passes/memo.cc` | New file: memo pass (split, toposort, dependency analysis) |
| `vbcc/CMakeLists.txt` | Add `passes/memo.cc` to `libvbcc` |
| `vbcc/main.cc` | Add `memo()` to pipeline |
| `vbcc/bytecode.cc` | Encode `MemoInit` list + `MemoLoad` opcode; FunctionId→index map |
| `vbci/program.h` | `memo_slots`, `memo_func_ids` |
| `vbci/program.cc` | Parse memo list; evaluate at startup |
| `vbci/thread.cc` | `MemoLoad` handler; Op debug name |
| `testsuite/v/` | New memo tests |

## Execution Order

1. **Part A** (`once` syntax) — parser, WF, reify emit of `FuncOnce`.
2. **Part B** (memo pass + runtime) — backend pass, bytecode, interpreter.
3. **Part C** (tests).
4. Golden file regeneration (`ninja update-dump-clean && ninja update-dump`).
