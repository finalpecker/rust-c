# Rust Compiler Roadmap

## Project Goal

Build a production-oriented Rust compiler in C with clear internal phases,
repeatable versioning, and incremental milestones.

The current codebase is a working subset compiler. The roadmap below defines
the path toward a more complete Rust implementation while keeping each stage
individually reviewable and reversible.

## Version Policy

- `v1.0.0` is the frozen teaching-oriented subset compiler snapshot.
- Mainline development continues from the current root workspace.
- Each milestone should be isolated in a git commit and, when the milestone is
  stable, mirrored into `versions/<tag>/`.

## Milestones

### Milestone 1: Repository and Architecture Hardening

- Initialize git history for the workspace.
- Keep the current runnable compiler as the baseline.
- Split the codebase into explicit compiler layers and internal headers.
- Add a small regression harness that can run in CI or locally.

### Milestone 2: Syntax and Name Resolution

- Expand the parser to cover more Rust surface syntax.
- Introduce a dedicated AST and symbol table layer.
- Add a resolver that separates lexical scopes from type checking.

### Milestone 3: Type System Expansion

- Add structs, enums, tuples, references, and function items.
- Build richer type representations and diagnostics.
- Prepare the compiler for ownership-aware analysis.

### Milestone 4: Ownership and Borrow Checking

- Model moves, copies, borrows, and lifetimes.
- Add borrow checking before lowering to the execution backend.
- Make diagnostics source-span aware and reproducible.

### Milestone 5: Lowering and Execution Backend

- Lower the validated program into an internal IR.
- Add a deterministic backend for code generation or interpretation.
- Expand runtime validation and error reporting.

### Milestone 6: Standard Library Surface

- Add a small teaching-oriented standard library layer.
- Support string handling, slices, and common collection primitives.
- Keep the implementation compact enough to remain auditable.

## Working Rules

- Do not remove the last known-good milestone without a replacement snapshot.
- Keep commits narrow and reviewable.
- Prefer mechanical refactors before semantic changes.
- Validate each milestone with the test suite before tagging a version.
