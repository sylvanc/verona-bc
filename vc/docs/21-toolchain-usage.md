# 21. Toolchain Usage

[← Table of Contents](README.md) | [Previous: Compiler Pipeline](20-compiler-pipeline.md) | [Next: Built-in Types →](22-builtin-types.md)

This chapter is a reference for the `vc` compiler and `vbci` interpreter command-line interfaces.

---

## 21.1 Compiling (`vc`)

### Basic Usage

```bash
vc build <source_dir>
```

Compiles all `.v` files in `<source_dir>` and produces `<dir_name>.vbc` in the current directory.

### Options

| Flag | Description |
|------|-------------|
| `-b <file>`, `--bytecode <file>` | Set the output bytecode filename |
| `-s`, `--strip` | Strip debug information from the bytecode |
| `-p <pass>`, `--pass <pass>` | Stop compilation after a specific pass |
| `--dump_passes=<dir>` | Dump intermediate ASTs to a directory |
| `-o <file>` | Output final AST (Trieste format) |

### Examples

```bash
# Compile with default output name
vc build my_project/                  # produces my_project.vbc

# Custom output
vc build my_project/ -b output.vbc

# Strip debug info
vc build my_project/ -s

# Stop after type inference (for debugging)
vc build my_project/ -p infer

# Dump all intermediate ASTs
vc build my_project/ --dump_passes=./dump/
```

### Output Naming

The output filename is derived from the source directory name:
- `vc build hello/` → `hello.vbc`
- `vc build my_project/` → `my_project.vbc`

Use `-b` to override.

### Important: Use the Installed Binary

Always use `build/dist/vc/vc`, not `build/vc/vc`. The installed binary has the `_builtin` standard library directory next to it, which the compiler requires for name resolution.

---

## 21.2 Running (`vbci`)

### Basic Usage

```bash
vbci <program.vbc>
```

Executes the bytecode file. The process exit code is the return value of `main()`.

### Options

| Flag | Description |
|------|-------------|
| `-t <N>`, `--threads <N>` | Number of scheduler threads (default: available CPU cores) |
| `-l <level>`, `--log_level <level>` | Set log level |

### Log Levels

`Trace`, `Debug`, `Info`, `Warning`, `Output`, `Error`, `None`

### Examples

```bash
# Run a program
vbci my_project.vbc

# Check the exit code
vbci my_project.vbc; echo $?

# Run with specific thread count
vbci my_project.vbc -t 4

# Debug logging
vbci my_project.vbc -l Debug
```

---

## 21.3 Development Workflow

### Build, Install, Test

```bash
cd build
ninja install                         # build and install to dist/
ctest --output-on-failure -j$(nproc)  # run test suite
```

### Compile and Run

```bash
cd build
dist/vc/vc build ../my_project/
dist/vbci/vbci my_project.vbc
echo $?                               # check exit code
```

### Debugging a Compilation Issue

```bash
# Dump ASTs to see where things go wrong
dist/vc/vc build ../my_project/ --dump_passes=./dump/

# Stop at a specific pass to inspect
dist/vc/vc build ../my_project/ -p ident

# Debug the interpreter with lldb
lldb-20 -- dist/vbci/vbci my_project.vbc
```

### Updating Test Golden Files

```bash
ninja update-dump-clean
ninja update-dump
cmake ..                              # re-run cmake to pick up new golden files
```

---

## 21.4 Test Conventions

Tests live in `testsuite/v/<name>/`:

```
testsuite/v/hello/
  hello.v                             # source file
  hello/compile/
    exit_code.txt                     # expected compile exit code (no trailing newline)
    stdout.txt                        # expected compile stdout (usually empty)
    stderr.txt                        # expected compile stderr (usually empty)
  hello/run/
    exit_code.txt                     # expected run exit code
    stdout.txt                        # expected run stdout
    stderr.txt                        # expected run stderr
```

Tests must be self-contained — no external dependencies. Use only `_builtin` types.
