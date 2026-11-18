# Changelog

## 1.0.1 - 2026-11-18

Safety release. No new features. Several commands now refuse ambiguous input
instead of guessing; scripts that relied on fuzzy `--delete` must pass an
exact id, exact display name, or a unique substring.

### Fixed

- **Destructive lookups (F01).** `--show`, `--rename`, `--delete` and
  `--replace-icon` resolve an entry by exact id, then exact display name
  (case-insensitive), then a unique substring. Empty and ambiguous queries
  exit 2, list the candidates, and change nothing.
- **Ownership (F02, interim).** Entries are managed only when they carry
  `Comment=Created by DejaTop`. A foreign `dejatop_*.desktop` file is no
  longer listed, renamed, or deleted.
- **No aborts (F03).** Filesystem errors are caught at the top level and exit
  1 with a message. Generated ids longer than the filename limit are
  truncated with a stable hash suffix.
- **Runner and target validation (F05).** `--runner` accepts only
  `native`, `wine` or `proton`; the target must be a regular file.
- **Environment keys (F09).** `--env` keys must match
  `[A-Za-z_][A-Za-z0-9_]*` and are rejected before anything is written.
- **Desktop value escaping (F10).** `Name=` is written through one escaper on
  create and rename, so backslashes, newlines, tabs and carriage returns
  round-trip and cannot inject a second key.
- **Piped output (F11).** Output is flushed before every fork; lines are no
  longer duplicated when stdout is redirected.
- **Permissions (F15).** Native targets gain owner-execute only (600 -> 700,
  not 711), and the change is reported.
- **Output streams and colour (F22).** Errors go to stderr. Colour is used
  only on a TTY and is disabled by `NO_COLOR`.
- **Option parsing (F25).** Options may appear before, after or around the
  target; `--` ends option parsing; missing values and unknown options are
  usage errors (exit 2).

### Documentation

- Removed the unimplemented `--update` from the README and bash completion.
- Documented runner override, `--env` key rules, and exit codes.

### Tests

- `tests/` acceptance suite (failing test per finding), `make test`,
  `make check`, header dependency tracking, and a CI workflow with gcc,
  clang, ASan and UBSan jobs.
