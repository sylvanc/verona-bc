# 24. Error Handling

[← Table of Contents](README.md) | [Previous: Grammar Summary](23-grammar-summary.md) | [Next: Compile-Time Execution →](25-compile-time-execution.md)

---

## 24.1 Raise

`raise` performs a non-local return from a block lambda back to the enclosing function. The raised value is returned as the result of the enclosing call expression. Runtime errors (type mismatches, stack escapes, etc.) are fatal to the behavior.

---

## 24.2 Current State

Error handling mechanisms:
- `raise` for non-local return from block lambdas
- Returning `nomatch` from functions to signal failure (see [Types](03-types.md))
- Using `else` on `for` loop iterations to handle iterator exhaustion
- Process exit codes from `main()`
- Runtime errors are fatal to the behavior
