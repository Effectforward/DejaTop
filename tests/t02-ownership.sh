#!/usr/bin/env bash
# F02 (interim): only entries DejaTop created may be listed or edited.
# Ownership is the Comment=Created by DejaTop marker, not the filename prefix.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"
create_entry "Mine" "$S"

FOREIGN="$APPS/dejatop_foo.desktop"
cat >"$FOREIGN" <<'EOF'
[Desktop Entry]
Type=Application
Name=NotMine
Exec=true
EOF

expect_rc 0 "--list succeeds with a foreign file present" --list
assert_out_contains "Mine" "--list shows the managed entry"
assert_out_not_contains "NotMine" "--list ignores the foreign file"

expect_rc 1 "--show ignores a foreign file" --show notmine
expect_rc 1 "--rename ignores a foreign file" --rename notmine Hijacked
expect_rc 1 "--replace-icon ignores a foreign file" --replace-icon notmine /bin/true
expect_rc 1 "--delete ignores a foreign file" --delete notmine
assert_file_exists "$FOREIGN" "foreign file survived every management command"

# Listing is deterministic.
create_entry "Zeta" "$S"
create_entry "Alpha" "$S"
run_dt --list
first=$(printf '%s\n' "$OUT" | grep -E '^  - ' | head -1)
last=$(printf '%s\n' "$OUT" | grep -E '^  - ' | tail -1)
case "$first" in *Alpha*) ok "listing is sorted (Alpha first)" ;; *) bad "listing is sorted (first: $first)" ;; esac
case "$last" in *Zeta*) ok "listing is sorted (Zeta last)" ;; *) bad "listing is sorted (last: $last)" ;; esac

# A managed entry still deletes normally.
expect_rc 0 "--delete removes a managed entry" --delete mine
assert_file_absent "$APPS/dejatop_Mine.desktop" "managed entry was removed"
assert_file_exists "$FOREIGN" "foreign file still untouched"

finish
