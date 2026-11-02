# STM32C5 Repository Rules

This repository contains reproducible teaching and experiment projects for the NUCLEO-C542RC (MB2213) board. Read this file before changing files.

## Operating rules

- Work on one small, verifiable goal at a time.
- Inspect `git status`, `.codex/project.yaml`, the relevant `lab.yaml`, and `docs/10-context-handoff.md` before implementation.
- Treat chip and board capabilities as unverified until supported by official documentation, CubeMX, or a recorded hardware test.
- Preserve completed experiments and unrelated user changes.
- Keep vendor/generated code separate from project-owned code.
- Keep dependency direction toward lower layers: vendor -> platform -> drivers -> middleware/services -> RTOS/OSAL -> apps/labs.
- Do not put application policy in reusable drivers.
- Do not commit secrets, tokens, private keys, or access-bearing URLs.
- A task is complete only after the appropriate build/test or an explicitly documented hardware blocker.
- Update lab metadata, documentation, and handoff state after meaningful changes.

## Required completion report

Use this structure in the final report and in `docs/10-context-handoff.md`: Goal, Changed, Verified, Hardware Result, Known Issues, Git Commit, Next Single Action.

## Git policy

Use `main` for verified releases, `develop` for integration, and scoped `feature/*` or `lab/*` branches for active work. Prefer small conventional commits.
