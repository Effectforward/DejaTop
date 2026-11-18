#!/usr/bin/env bash
# F01: destructive fuzzy matching. Lookup must be exact id -> exact name ->
# unique substring, and must refuse empty or ambiguous queries without
# destroying anything.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"

create_entry "Portal" "$S"
create_entry "Portal 2" "$S"
assert_count "$(desktop_count)" 2 "setup: two entries created"

# Exact name beats a longer name that contains it.
expect_rc 0 "--delete portal matches the exact name 'Portal'" --delete portal
assert_file_absent "$APPS/dejatop_Portal.desktop" "Portal was deleted"
assert_file_exists "$APPS/dejatop_Portal2.desktop" "Portal 2 survived an exact-name delete"

create_entry "Portal" "$S"

# Empty query must never guess.
expect_rc 2 "--delete '' is rejected" --delete ""
assert_count "$(desktop_count)" 2 "empty query deleted nothing"

# Ambiguous substring must list candidates and change nothing.
expect_rc 2 "--delete por is rejected as ambiguous" --delete por
assert_count "$(desktop_count)" 2 "ambiguous query deleted nothing"
case "$OUT$ERR" in
    *Portal*Portal\ 2*|*Portal\ 2*Portal*) ok "ambiguous query lists both candidates" ;;
    *) bad "ambiguous query lists both candidates (stdout: $OUT; stderr: $ERR)" ;;
esac

# Case-insensitive exact name.
expect_rc 0 "--delete pOrTaL is case-insensitive" --delete pOrTaL
assert_file_absent "$APPS/dejatop_Portal.desktop" "case-insensitive delete removed Portal"
assert_file_exists "$APPS/dejatop_Portal2.desktop" "case-insensitive delete kept Portal 2"

create_entry "Portal" "$S"

# Exact id is the first tier.
expect_rc 0 "--delete Portal2 matches the entry id" --delete Portal2
assert_file_absent "$APPS/dejatop_Portal2.desktop" "id lookup deleted Portal 2"
assert_file_exists "$APPS/dejatop_Portal.desktop" "id lookup kept Portal"

# With one match left, a substring is safe.
expect_rc 0 "--delete por now has a unique substring match" --delete por
assert_file_absent "$APPS/dejatop_Portal.desktop" "unique substring deleted Portal"

# A missing query is an operational failure, not a crash.
expect_rc 1 "--delete does-not-exist reports not found" --delete does-not-exist
assert_count "$(desktop_count)" 0 "nothing remains"

finish
