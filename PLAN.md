# DejaTop remediation plan

Source: `dejatop_architecture_review.pdf` (11 Nov 2026), review of v1.0.0.
This plan follows the review's phase order: tests first, then the P0s and the
cheap P1s. Each phase is shippable on its own.

## Working rules

- Every fix starts with a failing test in `tests/` (Red), then the smallest
  change that makes it pass (Green), then a cleanup pass.
- Tests run in a throw-away `$HOME`; no real user configuration is touched.
- Generated files are checked with the same oracles as the review:
  `desktop-file-validate`, GLib `Gio.DesktopAppInfo`, and wrapper execution.
- Deferred work is listed explicitly. Nothing in a later phase is pulled in
  because it is convenient.

## Phase 0 - Safety net (done, 2026-11-18)

| Task | Detail | Status |
|---|---|---|
| P0.1 | `tests/lib.sh` harness: temp `$HOME`, fake Proton, assertion helpers, cleanup | done |
| P0.2 | Acceptance tests for each Phase 1 finding (`tests/t*.sh`) | done |
| P0.3 | `make test` runs the suite; `make check` builds + tests | done |
| P0.4 | Makefile header dependencies (`-MMD -MP`), `-Wextra` | done |
| P0.5 | CI workflow: gcc + clang, `-Wall -Wextra`, tests, ASan/UBSan job | done |
| P0.6 | Keep the review bundle in sync (tests are the living version) | ongoing |

Exit: `make check` green on the new harness; every Phase 1 test fails against
v1.0.0 for the reason the review documented.

## Phase 1 - Stop the bleeding (v1.0.1, done 2026-11-18)

| ID | Fix | Test | Status |
|---|---|---|---|
| F01 | Deterministic lookup: exact id -> exact name (ci) -> unique substring; empty/ambiguous query exits 2 with candidates; nothing destroyed | `t01-lookup-delete.sh` | done |
| F02 | Interim ownership marker: only entries with `Comment=Created by DejaTop` are listed/edited/deleted | `t02-ownership.sh` | done |
| F03 | Top-level exception guard; no `std::terminate`; cap generated ids (slug + hash suffix) | `t03-abort.sh` | done |
| F05 | Validate `--runner` enum; require a regular file target; clear usage errors | `t05-runner.sh` | done |
| F09 | Validate `--env` keys against `[A-Za-z_][A-Za-z0-9_]*`; reject before writing | `t06-env.sh` | done |
| F10 | One desktop-value sanitiser for every key write, including rename | `t07-rename-escape.sh` | done |
| F11 | Flush stdout/stderr before fork; no duplicate output when piped | `t08-output.sh` | done |
| F15 | Add owner-execute only when needed; report the change | `t09-chmod.sh` | done |
| F22 | Errors on stderr; colour only on a TTY and never with `NO_COLOR` | `t08-output.sh` | done |
| F25 | Real option parser: any flag order, `--`, missing values are usage errors | `t10-cli.sh` | done |
| F12 (docs) | Remove `--update` claims from README/completion until it exists | `t11-docs.sh` | done |

Exit criteria: P0 tests pass; hostile corpus never aborts; output identical
when piped; `desktop-file-validate` clean for the Phase 1 corpus; release
v1.0.1.

## Phase 2 - Artifact correctness (v1.1)

F04 content-based runner sniffing; F06 two-layer escaping; F07 launch options
for non-Proton entries; F14 checked `cd`; F16 artifact cleanup; F18 immutable
slug+hash ids; F23 log cap; F24 categories; F31 Exec quoting for reserved
characters; F33 `XDG_DATA_HOME`; F35 runner/tool checks. Acceptance tests
T05-T18 from the review.

## Phase 3 - State model + Proton policy (v1.2)

F02 final marker; F12 `--update`; F13 pinned Proton and static wrapper;
F17 plan/apply with atomic writes; F19 pre-release-aware ranking. Gate A:
pinned-with-fallback vs dynamic. Gate B: optional umu backend.

## Phase 4 - Discovery adapters

F08 strict VDF reader; F20 Heroic/Lutris/prefix layout; F21 icon pipeline;
F32 icon selection by pixel size.

## Phase 5 - Features and distribution (v2.0)

F26 install layout/CI; F27 man page from option table; F28 options/actions;
F29 non-TTY prompt; F30 Flatpak Steam; F34 umu backend; F36 completions;
`--dry-run`, `--batch`.

## Non-goals

DejaTop stays a desktop-entry generator: no prefix manager, Proton downloader,
or launcher. Prefix and Proton lifecycle belong to Steam, Heroic, Lutris, umu.
