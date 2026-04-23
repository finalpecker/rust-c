# rust-c Workspace

This workspace contains two separate Rust compiler tracks implemented in C:

- `teaching/` is the frozen, educational subset compiler.
- `full/` is the engineering track for the production-oriented compiler.

Each track has its own build script, source tree, and test set.

## Version Policy

- `versions/v1.0.0` captures the original teaching compiler baseline.
- `versions/v1.1.0-dev1` captures the first modularized checkpoint.
- New milestones should be committed in git and mirrored into a dedicated
  version snapshot before the next major architectural change.

## Build

Build a specific track from the workspace root:

```powershell
.\build.ps1 -Project teaching
.\build.ps1 -Project full
```

You can also build from inside a track directory if you want to work on it in
isolation.

## Repository Account

When configuring git links or GitHub remotes, use the currently authenticated
GitHub account in your environment. If the environment does not expose that
account, do not guess it; resolve it through the authenticated tooling before
publishing links.
