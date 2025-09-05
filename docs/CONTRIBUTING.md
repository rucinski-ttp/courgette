# Contributing

Workflow

- Use feature branches and focused PRs.
- Keep commits atomic and messages descriptive.
- Update `AGENTS.md` when changing conventions or structure.

Coding Standards

- Run `scripts/format.sh` before pushing.
- Run `scripts/lint.sh` and fix reported issues where applicable.
- Prefer portable code in `lib/` so it can be host-tested.

Testing

- `scripts/test_unit.sh` must be green.
- Integration tests should validate serial output and major flows.

