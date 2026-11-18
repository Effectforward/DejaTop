#!/usr/bin/env bash
# Shared harness for DejaTop acceptance tests.
# Every test runs in a throw-away $HOME; no real user configuration is touched.
#
# Usage:  source "$(dirname "$0")/lib.sh"
# Requires a built binary at ../dejatop (or set D=/path/to/dejatop).

set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
D=${D:-$ROOT/dejatop}
TEST_NAME=$(basename "${BASH_SOURCE[1]:-$0}" .sh)
WORK=$(mktemp -d "${TMPDIR:-/tmp}/dejatop-test.XXXXXX") || exit 1

export HOME="$WORK/home"
unset NO_COLOR 2>/dev/null || true
unset XDG_DATA_HOME 2>/dev/null || true
mkdir -p "$HOME/.local/share/applications" "$HOME/Games"

APPS="$HOME/.local/share/applications"
export WRAPS="$HOME/.local/share/DejaTop/wrappers"

GOOD=0
BAD=0
RC=0
OUT=""
ERR=""
RESTORE_PERMS=""

cleanup() {
    if [ -n "$RESTORE_PERMS" ]; then chmod 755 "$RESTORE_PERMS" 2>/dev/null || true; fi
    rm -rf "$WORK"
}
trap cleanup EXIT

ok()   { GOOD=$((GOOD + 1)); printf 'ok   - %s\n' "$*"; }
bad()  { BAD=$((BAD + 1));  printf 'FAIL - %s\n' "$*"; }
note() { printf '# %s\n' "$*"; }

finish() {
    printf '# %s: %d passed, %d failed\n' "$TEST_NAME" "$GOOD" "$BAD"
    [ "$BAD" -eq 0 ]
}

# Run the binary with stdin closed, capturing stdout/stderr and the exit code.
run_dt() {
    "$D" "$@" >"$WORK/out" 2>"$WORK/err" < /dev/null
    RC=$?
    OUT=$(cat "$WORK/out")
    ERR=$(cat "$WORK/err")
}

expect_rc() { # expected description command...
    local want=$1 desc=$2
    shift 2
    run_dt "$@"
    if [ "$RC" -eq "$want" ]; then
        ok "$desc"
    else
        bad "$desc (expected rc $want, got $RC; stderr: $(head -c 200 "$WORK/err" | tr '\n' ' '))"
    fi
}

assert_out_contains() {
    case "$OUT" in
        *"$1"*) ok "$2" ;;
        *)      bad "$2 (stdout: $(printf '%s' "$OUT" | head -c 200 | tr '\n' ' '))" ;;
    esac
}

assert_err_contains() {
    case "$ERR" in
        *"$1"*) ok "$2" ;;
        *)      bad "$2 (stderr: $(printf '%s' "$ERR" | head -c 200 | tr '\n' ' '))" ;;
    esac
}

assert_out_not_contains() {
    case "$OUT" in
        *"$1"*) bad "$2 (unexpected '$1' on stdout)" ;;
        *)      ok "$2" ;;
    esac
}

assert_out_empty() {
    if [ -z "$OUT" ]; then ok "$1"; else bad "$1 (stdout: $(printf '%s' "$OUT" | head -c 200 | tr '\n' ' '))"; fi
}

assert_err_empty() {
    if [ -z "$ERR" ]; then ok "$1"; else bad "$1 (stderr: $(printf '%s' "$ERR" | head -c 200 | tr '\n' ' '))"; fi
}

assert_file_exists() { if [ -f "$1" ]; then ok "$2"; else bad "$2 ($1 missing)"; fi; }
assert_file_absent() { if [ ! -e "$1" ]; then ok "$2"; else bad "$2 ($1 still exists)"; fi; }

assert_file_contains() {
    if grep -qF -- "$2" "$1" 2>/dev/null; then ok "$3"; else bad "$3 ('$2' not in $1)"; fi
}

assert_file_not_contains() {
    if grep -qF -- "$2" "$1" 2>/dev/null; then bad "$3 ('$2' found in $1)"; else ok "$3"; fi
}

assert_line_count() { # file pattern expected description
    local got
    got=$(grep -cE -- "$2" "$1" 2>/dev/null || true)
    if [ "$got" -eq "$3" ]; then ok "$4"; else bad "$4 (expected $3 '$2' lines, got $got)"; fi
}

assert_count() { # actual expected description
    if [ "$1" -eq "$2" ]; then ok "$3"; else bad "$3 (expected $2, got $1)"; fi
}

assert_perms() { # path expected description
    local got
    got=$(stat -c '%a' "$1" 2>/dev/null || echo missing)
    if [ "$got" = "$2" ]; then ok "$3"; else bad "$3 (expected mode $2, got $got)"; fi
}

assert_no_ansi() { # file description
    if LC_ALL=C grep -q $'\x1b' "$1" 2>/dev/null; then bad "$2 (ANSI escapes present)"; else ok "$2"; fi
}

desktop_count() {
    find "$APPS" -maxdepth 1 -name 'dejatop_*.desktop' 2>/dev/null | wc -l | tr -d ' '
}

mk_script() { # path [mode]
    mkdir -p "$(dirname "$1")"
    printf '#!/bin/sh\nexit 0\n' > "$1"
    chmod "${2:-755}" "$1"
}

# Create an entry and fail loudly if setup breaks.
create_entry() { # name path [extra args...]
    local name=$1 path=$2
    shift 2
    run_dt "$path" --name "$name" --force "$@" || true
    if [ "$RC" -ne 0 ]; then
        bad "setup: create '$name' failed (rc=$RC; stderr: $(head -c 200 "$WORK/err" | tr '\n' ' '))"
        return 1
    fi
}

fakeproton() {
    local v d
    for v in "$@"; do
        d="$HOME/.steam/root/compatibilitytools.d/$v"
        mkdir -p "$d"
        printf '#!/bin/sh\necho "PROTON[$0] args: $*"\necho "DATA=$STEAM_COMPAT_DATA_PATH"\n' > "$d/proton"
        chmod +x "$d/proton"
    done
}

fake_tool() { # name [exit-code]
    mkdir -p "$WORK/bin"
    printf '#!/bin/sh\nexit %s\n' "${2:-0}" > "$WORK/bin/$1"
    chmod +x "$WORK/bin/$1"
    case ":$PATH:" in
        *":$WORK/bin:"*) ;;
        *) PATH="$WORK/bin:$PATH"; export PATH ;;
    esac
}

glib_name() { # desktop-file -> display name, or empty if GLib rejects it
    python3 - "$1" <<'PY' 2>/dev/null
import sys
try:
    import gi
    gi.require_version('Gio', '2.0')
    from gi.repository import Gio
except Exception:
    sys.exit(3)
app = Gio.DesktopAppInfo.new_from_filename(sys.argv[1])
print(app.get_name() if app else '')
PY
}

have_glib() { python3 -c "import gi; gi.require_version('Gio','2.0')" >/dev/null 2>&1; }
