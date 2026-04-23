# Mini Rust Compiler in C

This project implements a teaching-oriented Rust subset compiler in C.
It is designed for compiler-principles education: the code is intentionally
structured, commented, and split into clear phases.

## What It Supports

- Integer and boolean literals
- Immutable and mutable local bindings
- Arithmetic, comparison, and boolean operators
- `if` / `else` expressions
- `while` expressions
- `break` and `continue` statements in loops
- Blocks and block expressions
- Function definitions and function calls
- `return` statements
- Static type checking for `i64`, `bool`, and `()`

## What It Does Not Support

This is not a full Rust compiler.
It does not implement ownership, lifetimes, traits, structs, enums, generics,
modules, pattern matching, references, or the borrow checker.

## Build

On Windows with MinGW GCC:

```powershell
.\build.ps1
```

The compiler binary is written to `build\mini-rustc.exe`.

## Run a Program

```powershell
.\build\mini-rustc.exe .\tests\samples\recursion.rx
```

The compiler executes the compiled bytecode and prints the `main` result.
Successful runs exit with code `0`; semantic or runtime errors return a
non-zero exit code.

## Run the Test Suite

```powershell
.\tests\run-tests.ps1
```

The suite includes:

- Arithmetic and precedence checks
- Boolean logic and comparisons
- Mutable state and loops
- Loop control with `break` / `continue`
- Recursive calls
- Block expressions and shadowing
- Negative cases for semantic errors

## Notes for Teaching

The code is organized to make the compiler pipeline easy to follow:

1. Entry point in `src/main.c`
2. Compiler core in `src/compiler.c`
3. Public API declaration in `src/compiler.h`
4. Compiler stages (lexer -> parser -> semantic analysis -> bytecode VM)

That flow is deliberate so the project can be used to teach compiler structure
without relying on an existing Rust compiler implementation.
