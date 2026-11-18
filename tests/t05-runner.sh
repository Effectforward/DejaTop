#!/usr/bin/env bash
# F05: --runner must be validated and targets must be regular files.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"
mkdir -p "$HOME/Games/adir"
: >"$HOME/Games/game.exe"
fakeproton GE-Proton10-34

expect_rc 2 "--runner banana is a usage error" "$S" --runner banana --name Bad
assert_count "$(desktop_count)" 0 "invalid runner created nothing"
assert_err_contains "runner" "runner error names the bad option"

expect_rc 1 "a directory is not a valid target" "$HOME/Games/adir" --name Dir
assert_count "$(desktop_count)" 0 "directory target created nothing"

expect_rc 1 "a missing target is an error" "$HOME/Games/nope" --name Nope
expect_rc 2 "--runner without a value is a usage error" "$S" --runner

expect_rc 0 "a regular file is accepted" "$S" --runner native --name Good
assert_file_exists "$APPS/dejatop_Good.desktop" "valid native entry was created"

# An explicit override wins even when the extension suggests otherwise.
expect_rc 0 "--runner proton overrides a .sh target" "$S" --runner proton --name Override
assert_file_exists "$APPS/dejatop_Override.desktop" "explicit proton override was created"

expect_rc 0 "--runner wine is valid" "$S" --runner wine --name WineOk
assert_file_exists "$APPS/dejatop_WineOk.desktop" "wine entry was created"

finish
