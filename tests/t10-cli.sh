#!/usr/bin/env bash
# F25: option parsing accepts any order and `--`; missing values and unknown
# flags are usage errors with a clear message.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"

# Flags before the path.
expect_rc 0 "flags may precede the target" --name Cli --force "$S"
assert_file_exists "$APPS/dejatop_Cli.desktop" "flags-first creation worked"

# Flags interleaved and after the path.
expect_rc 0 "flags may be interleaved" --env A=1 --name Ordered --runner native "$S" --force
assert_file_exists "$APPS/dejatop_Ordered.desktop" "interleaved flags worked"

expect_rc 0 "flags may follow the target" "$S" --force --name After
assert_file_exists "$APPS/dejatop_After.desktop" "trailing flags worked"

# -- ends option parsing.
expect_rc 0 "-- terminates option parsing" --force -- "$S"
assert_file_exists "$APPS/dejatop_start.desktop" "-- separator worked"

# A target whose name starts with '-' works after --.
mk_script "$HOME/Games/-dash.sh"
( cd "$HOME/Games" && "$D" --force -- -dash.sh >"$WORK/dash.out" 2>"$WORK/dash.err" </dev/null )
dash_rc=$?
assert_count "$dash_rc" 0 "a leading-dash target works after --"
assert_no_ansi "$WORK/dash.out" "leading-dash creation output is clean"

# Missing values are usage errors, not "Unknown option".
expect_rc 2 "--name without a value is a usage error" "$S" --name
assert_err_contains "requires a value" "--name error explains the missing value"
expect_rc 2 "--force is not silently accepted as a value" "$S" --name --force
expect_rc 2 "--runner without a value is a usage error" "$S" --runner
expect_rc 2 "--env without a value is a usage error" "$S" --env

# Unknown flags are usage errors.
expect_rc 2 "unknown long flag is a usage error" "$S" --bogus
assert_err_contains "Unknown option" "unknown flag names the option"
expect_rc 2 "unknown short flag is a usage error" "$S" -x

# Bare commands and help.
expect_rc 1 "bare --show prints help" --show
assert_out_contains "Usage" "bare --show shows usage"
expect_rc 0 "--help succeeds" --help
assert_out_contains "--runner" "--help lists creation options"
expect_rc 0 "--version succeeds" --version
assert_out_contains "DejaTop v" "--version prints the version"
expect_rc 0 "no arguments prints help"
assert_out_contains "Usage" "no-argument help"

finish
