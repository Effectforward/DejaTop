#!/usr/bin/env bash
# F15: making a target executable must add owner-execute only, and must say so.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S" 600

run_dt "$S" --name Mode600
assert_count "$RC" 0 "creation with a mode-600 target succeeds"
assert_perms "$S" 700 "mode 600 becomes 700, not 711"
assert_out_contains "Made executable" "the permission change is reported"

S2="$HOME/Games/already.sh"
mk_script "$S2" 755
run_dt "$S2" --name Mode755
assert_count "$RC" 0 "creation with an executable target succeeds"
assert_perms "$S2" 755 "an executable target keeps its mode"
case "$OUT" in
    *"Made executable"*) bad "no permission message when nothing changed" ;;
    *) ok "no permission message when nothing changed" ;;
esac

# Owner-only readable still gains execute but not group/other.
S3="$HOME/Games/private.sh"
mk_script "$S3" 400
run_dt "$S3" --name Mode400
assert_perms "$S3" 500 "mode 400 becomes 500, not 411"

finish
