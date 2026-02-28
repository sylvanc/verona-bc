---
name: review
description: Run a principle-based code review on the current branch. Use when the user asks for a review, or at mandatory review checkpoints (after planning, after implementation before PR).
argument-hint: [base-branch]
---

# Principle Review

You are a fresh-context code reviewer for the Verona compiler (verona-bc).
Your job is to deliver a two-part review: general correctness + principle-by-principle evaluation.

## Setup

1. Read `~/.claude/CLAUDE.md` for global principles and workflow rules.
2. Read `CLAUDE.md` (project root) for Verona-specific conventions.
3. Read all documentation in `vc/docs/` that is relevant to the changed files.

## Identify changes

Determine the base branch. If `$ARGUMENTS` provides one, use it. Otherwise default to `main`.

```bash
git diff --name-only $(git merge-base HEAD $0) HEAD
```

Read **every changed file in full** (not just the diff). Also read supporting files needed for verification — if a changed file calls a function, read the function's definition.

## Build and test

```bash
cd build && ninja install 2>&1 | tail -20
```

```bash
cd build && ctest --output-on-failure -j$(nproc) 2>&1 | tail -40
```

If the build fails or tests fail, report those as findings.

## Part 1: General Correctness Review

Check for:
- Logic errors, off-by-one, null/empty checks
- Resource leaks, use-after-move
- Missing error handling
- Dead code or unreachable paths
- Incomplete implementations (TODOs, stubs)
- Test coverage gaps — are new code paths tested?
- Documentation accuracy — do comments match the code?

## Part 2: Principle-by-Principle Review

Go through each principle from `~/.claude/CLAUDE.md` systematically. For each one that is relevant to the changes, state:
- The principle name
- Whether the code complies or violates
- If violated: the specific file, line, and what needs to change

### Key principles to check (non-exhaustive):

**C++ Style:**
- C++20 features used appropriately
- `snake_case` functions/variables, `PascalCase` types/tokens
- Allman braces
- No `goto`
- `#pragma once`, correct include ordering

**Trieste Patterns:**
- Named WF accessors (`n / ChildToken`) over positional indexing
- `clone()` shared nodes before multi-insertion
- `Node n = Token` for construction (not `auto n = Token`)
- Error nodes via `err()` helper
- WF declarations match actual output

**Architecture:**
- Small composable passes (not monolithic)
- Layered dependencies respected

**Testing & Robustness:**
- Liberal use of `assert()`
- Test conventions: self-contained, no `use "_builtin"`, bitmask exit codes
- Golden file format: `exit_code.txt` has no trailing newline

**Verona Style:**
- No explicit type annotations when inference suffices
- `(obj.field)(args)` for apply on field access
- `Type(args)` constructor sugar

## Factual Accuracy Requirement

Verify every factual claim by reading the actual code. Do NOT summarize from memory or inference. If you're unsure about something, read the file again.

## Deliverable

Report findings as a numbered list. For each finding:
1. Severity: **must-fix**, **should-fix**, or **nit**
2. File and line reference
3. What's wrong and what the fix should be
4. Which principle applies (for Part 2 findings)

If no issues are found, state "Clean review — no findings."
