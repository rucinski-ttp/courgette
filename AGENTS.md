# Agent Guide

This repository is organized for AI-agent-led development. Agents should follow
these principles to collaborate effectively and keep the project maintainable.

Guiding Principles

- Clarity: Prefer small, reviewable changes; document assumptions and outcomes.
- Safety: Avoid destructive actions; never rewrite history without approval.
- Reproducibility: Use scripts in `scripts/` and docs in `docs/` — do not rely
  on local machine state. Always note versions and environment requirements.
- Verification: Build, flash, and run tests after meaningful changes. Capture
  logs and artifacts in `build/`.
- Modularity: Isolate platform-specific code under `app/src/platform` and
  drivers under `app/src/drivers`. Keep portable logic in `lib/`.
- Testability: Each folder should build as a library target; add unit tests in
  `tests/unit/` and integration tests in `tests/pytest/`.
- Iteration: Prefer scaffolding and placeholders to unblock parallel work; mark
  TODOs with actionable next steps.

This file will be updated as we converge on additional conventions.

