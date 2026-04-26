# Rust Compiler Engineering Track

This project is the engineering track for a Rust compiler implemented in C.
It is intentionally kept separate from the teaching-oriented compiler under
`teaching/` so the two lines can evolve independently.

The current code is the inherited baseline from the teaching track, but this
directory is the place where the production-oriented compiler will evolve.

## Current Baseline

- Integer and boolean literals
- Immutable and mutable local bindings
- Arithmetic, comparison, and boolean operators
- `if` / `else` expressions
- `while` expressions
- `loop { ... }` expressions
- `break` and `continue` statements in loops
- `for <name> in <start>..<end> { ... }` range loops
- Named `struct` definitions and field access
- Named `enum` definitions and `match` expressions (MVP)
- References (`&` / `&mut`) and dereference (`*`)
- Blocks and block expressions
- Function definitions and function calls
- `return` statements
- Static type checking for `i64`, `bool`, `()`, named struct types, and reference types
- Borrow checker MVP for local bindings (mutable/immutable borrow conflict detection)

## Target Scope

The engineering track aims to move toward a practical Rust implementation with
these milestones:

- Stronger parsing and name resolution
- A richer type system with structs, enums, tuples, and references
- Ownership, moves, borrowing, and lifetime checking
- An internal IR that is easier to validate and optimize
- Better diagnostics and a more maintainable module layout

The teaching track remains the place for the minimal pedagogical subset.

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

## Roadmap

See [docs/roadmap.md](docs/roadmap.md) for the staged implementation plan.

## Run the Test Suite

```powershell
.\tests\run-tests.ps1
```

The suite includes:

- Arithmetic and precedence checks
- Boolean logic and comparisons
- Mutable state and loops
- Loop control with `break` / `continue`
- Named struct construction and field projection
- Enum variant values and exhaustive match checking
- Borrow and dereference behaviors
- Recursive calls
- Block expressions and shadowing
- Negative cases for semantic errors

## Notes for Engineering

The code is being reorganized to make the compiler pipeline easy to extend:

1. Entry point in `src/main.c`
2. Shared utilities in `src/support.c`
3. Public compiler API in `src/compiler.c` and `src/compiler.h`
4. Compiler pipeline implementation in `src/pipeline.c` and `src/pipeline.h`
5. Compiler stages (lexer -> parser -> semantic analysis -> codegen -> VM)

That flow is deliberate so the project can grow toward a complete compiler
without losing the ability to validate each stage independently.
