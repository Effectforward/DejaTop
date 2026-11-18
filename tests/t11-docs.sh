#!/usr/bin/env bash
# F12 (docs): do not advertise commands that do not exist.
source "$(dirname "$0")/lib.sh"

ROOT=$(cd "$(dirname "$0")/.." && pwd)

if grep -q -- '--update' "$ROOT/README.md" "$ROOT/completions/dejatop.bash" "$ROOT/dejatop.1" 2>/dev/null; then
    bad "no documentation advertises the unimplemented --update"
else
    ok "no documentation advertises the unimplemented --update"
fi

run_dt --help
for opt in --name --icon --runner --prefix --category --env --force; do
    assert_out_contains "$opt" "--help documents $opt"
done
for cmd in --show --rename --replace-icon --delete --list; do
    assert_out_contains "$cmd" "--help documents $cmd"
done

# README must not promise behaviour the code does not have.
if grep -qE -- '--runner.*(banana|invalid)' "$ROOT/README.md"; then
    bad "README does not document invalid runners"
else
    ok "README does not document invalid runners"
fi

finish
