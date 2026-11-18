#!/usr/bin/env bash
# F11 + F22: forking tools must not duplicate buffered output; errors go to
# stderr; colour is disabled when not on a TTY and when NO_COLOR is set.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"

# Both external tools present = the maximum number of forks.
fake_tool desktop-file-validate
fake_tool update-desktop-database

run_dt "$S" --name Piped
assert_count "$(grep -c 'Processing' "$WORK/out")" 1 "one 'Processing' line when stdout is redirected"
assert_no_ansi "$WORK/out" "stdout has no ANSI escapes when redirected"
assert_no_ansi "$WORK/err" "stderr has no ANSI escapes when redirected"
assert_file_exists "$APPS/dejatop_Piped.desktop" "entry was created"

# NO_COLOR is honoured even if a caller forces a colour-capable environment.
NO_COLOR=1 "$D" --version >"$WORK/nc.out" 2>&1 </dev/null
assert_no_ansi "$WORK/nc.out" "NO_COLOR suppresses colour"

# Errors belong on stderr, not stdout.
run_dt --delete does-not-exist
assert_out_empty "operational error writes nothing to stdout"
assert_err_contains "Error" "operational error is written to stderr"

run_dt "$HOME/Games/missing" --name Missing
assert_out_empty "missing target writes nothing to stdout"
assert_err_contains "Error" "missing target is written to stderr"

# Help and listings are clean when piped.
run_dt --help
assert_no_ansi "$WORK/out" "--help has no ANSI escapes when piped"
run_dt --list
assert_no_ansi "$WORK/out" "--list has no ANSI escapes when piped"

# Success still goes to stdout.
run_dt "$S" --name GoodOut
assert_out_contains "created successfully" "success message stays on stdout"

finish
