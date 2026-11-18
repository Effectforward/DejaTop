#!/usr/bin/env bash
# F10: every key write uses the same sanitiser. A newline in --rename must not
# create a second key or an injected Exec line, and backslashes must round-trip.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"

create_entry "Old" "$S"

NEW=$'Hax\nExec=/bin/evil'
expect_rc 0 "rename with a newline succeeds" --rename Old "$NEW"
MOVED=$(find "$APPS" -maxdepth 1 -name 'dejatop_Hax*.desktop' | head -1)
assert_file_exists "$MOVED" "renamed entry exists"
assert_line_count "$MOVED" '^Exec=' 1 "rename injects no second Exec= line"
assert_line_count "$MOVED" '^Exec=/bin/evil$' 0 "raw newline is not written as a real line"
assert_file_contains "$MOVED" 'Name=Hax\nExec=/bin/evil' "newline is escaped as \\n in Name="

create_entry "Back" "$S"
expect_rc 0 "rename with a backslash succeeds" --rename Back 'back\slash'
BACK="$APPS/dejatop_backslash.desktop"
assert_file_exists "$BACK" "backslash rename produced a valid id"
assert_file_contains "$BACK" 'Name=back\\slash' "backslash is doubled in Name="
assert_line_count "$BACK" '^Name=' 1 "backslash rename leaves one Name= line"

if have_glib; then
    got=$(glib_name "$BACK")
    if [ "$got" = 'back\slash' ]; then
        ok "GLib reads the escaped name correctly"
    else
        bad "GLib reads the escaped name correctly (got '$got')"
    fi
fi

create_entry "EmptyTest" "$S"
expect_rc 1 "rename to an empty name is refused" --rename EmptyTest ""
assert_file_exists "$APPS/dejatop_EmptyTest.desktop" "refused rename changed nothing"

expect_rc 1 "rename to symbols-only is refused" --rename EmptyTest '!!!'
assert_file_exists "$APPS/dejatop_EmptyTest.desktop" "symbols-only rename changed nothing"

create_entry "Collision" "$S"
expect_rc 1 "rename to an existing id is refused" --rename EmptyTest Collision
assert_file_exists "$APPS/dejatop_EmptyTest.desktop" "collision kept the source entry"
assert_file_exists "$APPS/dejatop_Collision.desktop" "collision kept the target entry"

finish
