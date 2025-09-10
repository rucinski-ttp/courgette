# Coding Standards

This repository follows these C/C++ coding conventions for all non-external code (app/** and lib/**).

- Braces: Always use braces for single‑statement `if/else/while/for` blocks.
- Early returns: Prefer a single return path per function where it improves clarity. Use a local `rc` and a single `out:` label for cleanup/unified exit when needed. Trivial guard‑clauses are acceptable.
- Private helpers: Mark file‑local helpers `static`.
- Unsigned shifts: Use unsigned shift amounts (e.g., `<< 8u`) and unsigned constants for bit operations.
- Bitwise on APIs/macros: Compose flags with unsigned intermediates and typed enums where available. For official Zephyr macros that trip static analysis (e.g., `GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW`, `BIT(n)`), it is acceptable to add a targeted `// NOLINT(hicpp-signed-bitwise)` at the specific line (no catch‑all suppressions).
- Copies/logging: Prefer safe bounded appends for debug string construction. Avoid raw `memcpy` for ad‑hoc string building in new code.
- Naming: Keep internal helpers descriptive and small; consider splitting large functions into smaller statics when it improves readability.
- Headers: Avoid ad‑hoc `extern` in source files; add proper headers instead.
- Formatting: clang‑format is applied with the project’s `.clang-format`. Current formatter version may not support `IndentExternBlock`; prefer header‑based `extern \"C\"` declarations and avoid deep extern blocks in sources.
- Lint: clang‑tidy should run clean on app/** and lib/** under `.clang-tidy`. Use targeted `NOLINT` only where Zephyr official macros cause unavoidable warnings.

See also:
- `scripts/lint.sh` for how lint runs (split DB for app/lib)
- `docs/PROTOCOL.md` and `docs/TESTING.md` for protocol and test guidance

