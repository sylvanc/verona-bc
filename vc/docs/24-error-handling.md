# 24. Error Handling

[← Table of Contents](README.md) | [Previous: Grammar Summary](23-grammar-summary.md) | [Next: Compile-Time Execution →](25-compile-time-execution.md)

> **Status:** Not yet implemented. The keywords `try`, `raise`, and `throw` are reserved for future use.

---

## 24.1 Planned Design

Error handling in Verona will use structured error propagation with `try`, `raise`, and `throw`. The exact semantics are under development.

**Reserved keywords:**
- `try` — begin an error-handling scope
- `raise` — propagate a recoverable error
- `throw` — propagate an unrecoverable error

---

## 24.2 Current State

Programs currently have no error handling mechanism beyond:
- Returning `nomatch` from functions to signal failure (see [Types](03-types.md))
- Using `else` on `for` loop iterations to handle iterator exhaustion
- Process exit codes from `main()`

This chapter will be expanded when error handling is implemented.
