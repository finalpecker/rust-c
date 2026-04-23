# Full Compiler Roadmap

## Goal

Build the engineering track for a Rust compiler in C, separated from the
teaching compiler under `teaching/`.

## Milestones

### Stage 1: Workspace Isolation

- Keep `teaching/` frozen as the pedagogical baseline.
- Move production work into `full/`.
- Maintain git commits and version snapshots for each stable checkpoint.

### Stage 2: Parser and Resolver

- Expand the parser to cover Rust surface syntax more completely.
- Add module-aware name resolution and import handling.
- Split AST construction from semantic validation.

### Stage 3: Type System

- Add structs, enums, tuples, function items, and references.
- Implement richer diagnostics for type mismatches and inference failures.

### Stage 4: Ownership Model

- Add move semantics and borrow checking.
- Track lifetimes in a compiler-internal representation.
- Emit source-span diagnostics with actionable messages.

### Stage 5: Lowering and Backend

- Lower from validated AST/HIR into a simpler IR.
- Keep the backend deterministic and testable.
- Add optimizations only after the semantics are stable.

### Stage 6: Ecosystem Surface

- Add a small standard library surface suitable for engineering use.
- Support string handling, slices, and containers as needed.

## Version Rules

- Tag stable milestones in git.
- Mirror stable milestones into `versions/`.
- Never overwrite a validated snapshot without creating a new checkpoint.
