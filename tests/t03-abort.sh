#!/usr/bin/env bash
# F03: ordinary failures must not abort. Long names are capped, and an
# unreadable directory produces a clean error, never std::terminate.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"

LONG=$(python3 -c 'print("N" * 5000)')
run_dt "$S" --name "$LONG" --force
if [ "$RC" -eq 134 ] || printf '%s' "$ERR" | grep -qi 'terminate called'; then
    bad "5000-char name does not abort"
else
    ok "5000-char name does not abort"
fi
if [ "$RC" -eq 0 ]; then
    ok "5000-char name creates an entry"
    f=$(find "$APPS" -maxdepth 1 -name 'dejatop_*.desktop' | head -1)
    if [ -n "$f" ] && [ "$(basename "$f" | wc -c)" -le 255 ]; then
        ok "generated filename is within the filesystem limit"
    else
        bad "generated filename is within the filesystem limit"
    fi
else
    bad "5000-char name creates an entry (rc=$RC; stderr: $(head -c 200 "$WORK/err"))"
fi

# Repeating the same long name is deterministic (hash suffix, not random).
first=$(find "$APPS" -maxdepth 1 -name 'dejatop_*.desktop' | head -1)
run_dt "$S" --name "$LONG" --force
second=$(find "$APPS" -maxdepth 1 -name 'dejatop_*.desktop' | head -1)
if [ -n "$first" ] && [ "$first" = "$second" ]; then
    ok "long id is stable across runs"
else
    bad "long id is stable across runs ($first vs $second)"
fi

# 300 chars is just over the filename limit once the prefix/suffix are added.
LONG300=$(python3 -c 'print("M" * 300)')
run_dt "$S" --name "$LONG300" --force
if [ "$RC" -eq 134 ] || printf '%s' "$ERR" | grep -qi 'terminate called'; then
    bad "300-char name does not abort"
else
    ok "300-char name does not abort"
fi

# Unreadable applications directory: clean failure, no abort.
before=$(desktop_count)
RESTORE_PERMS="$APPS"
chmod 000 "$APPS"
run_dt --list
if [ "$RC" -eq 134 ] || printf '%s' "$ERR" | grep -qi 'terminate called'; then
    bad "unreadable directory does not abort"
else
    ok "unreadable directory does not abort"
fi
if [ "$RC" -eq 1 ] && [ -n "$ERR" ]; then
    ok "unreadable directory exits 1 with a message on stderr"
else
    bad "unreadable directory exits 1 with a message on stderr (rc=$RC)"
fi
chmod 755 "$APPS"
# shellcheck disable=SC2034  # read by the EXIT trap in lib.sh
RESTORE_PERMS=""
assert_count "$(desktop_count)" "$before" "unreadable listing deleted nothing"

finish
