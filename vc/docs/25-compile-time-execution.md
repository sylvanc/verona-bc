# 25. Compile-Time Execution

[← Table of Contents](README.md) | [Previous: Error Handling](24-error-handling.md)

> **Status:** Not yet implemented. The `#` operator is reserved for compile-time execution.

---

## 25.1 Planned Design

The `#` prefix operator will mark an expression for evaluation at compile time. The exact semantics are under development.

```verona
// Future syntax (not yet implemented):
let table = #compute_lookup_table();
```

---

## 25.2 Current State

The `#` operator is recognized by the parser and handled in the application pass, but compile-time execution is not yet functional. This chapter will be expanded when the feature is implemented.
