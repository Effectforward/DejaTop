#!/usr/bin/env bash
# F09: --env keys are shell identifiers. A hostile key must be rejected before
# anything is written, and a malformed key must not silently become an export.
source "$(dirname "$0")/lib.sh"

S="$HOME/Games/start.sh"
mk_script "$S"
E="$HOME/Games/game.exe"
: >"$E"
fakeproton GE-Proton10-34
PWNED="$WORK/PWNED"

expect_rc 2 "hostile --env key is rejected" "$E" --name Inject --env 'A;touch '"$PWNED"';B=1'
assert_count "$(desktop_count)" 0 "hostile env created no entry"
assert_file_absent "$WRAPS/Inject_wrapper.sh" "hostile env created no wrapper"
assert_file_absent "$PWNED" "hostile env executed nothing"

expect_rc 2 "--env key with a space is rejected" "$S" --name BadKey --env 'BAD KEY=1'
expect_rc 2 "--env key starting with a digit is rejected" "$S" --name Digit --env '1BAD=1'
expect_rc 2 "--env without '=' is rejected" "$S" --name NoEq --env 'NOEQUALS'
expect_rc 2 "--env with an empty key is rejected" "$S" --name EmptyKey --env '=x'
assert_count "$(desktop_count)" 0 "rejected env values created nothing"

expect_rc 0 "valid env keys are accepted" "$S" --name GoodEnv --runner native \
    --env '_A=1' --env 'GOOD_KEY=2'
assert_file_exists "$APPS/dejatop_GoodEnv.desktop" "valid env entry was created"
assert_file_contains "$APPS/dejatop_GoodEnv.desktop" 'env _A=1 GOOD_KEY=2 ' "both env pairs are in Exec"

expect_rc 0 "an '=' in the value is allowed" "$S" --name EqValue --runner native --env 'A=b=c'
assert_file_contains "$APPS/dejatop_EqValue.desktop" 'A=b=c' "value keeps its inner '='"

expect_rc 0 "an empty value is allowed" "$S" --name EmptyVal --runner native --env 'A='
assert_file_contains "$APPS/dejatop_EmptyVal.desktop" 'env A= ' "empty value is exported"

finish
