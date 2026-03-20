#!/usr/bin/env bash
# Integration tests for fm (File Monitor)
# Usage: ./test/test.sh [path-to-fm-binary]
# Default binary: ./build/fm

set -euo pipefail

FM="${1:-./build/fm}"
TMPDIR_BASE="$(mktemp -d -p "$HOME" fm_test_XXXXXX)"
BASELINE="$TMPDIR_BASE/baseline.dat"
BASELINE2="$TMPDIR_BASE/baseline2.dat"
TESTDIR="$TMPDIR_BASE/testdir"

PASS=0
FAIL=0

cleanup() {
    rm -rf "$TMPDIR_BASE"
}
trap cleanup EXIT

# ----- helpers -----
pass() { echo "  PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL+1)); }

check() {
    local desc="$1"; shift
    local expected_exit="$1"; shift
    local actual_exit=0
    "$@" >/dev/null 2>&1 || actual_exit=$?
    if [ "$actual_exit" -eq "$expected_exit" ]; then
        pass "$desc"
    else
        fail "$desc (expected exit $expected_exit, got $actual_exit)"
    fi
}

check_output() {
    local desc="$1"; shift
    local expected_exit="$1"; shift
    local pattern="$1"; shift
    local actual_exit=0
    local out
    out=$("$@" 2>&1) || actual_exit=$?
    if [ "$actual_exit" -eq "$expected_exit" ] && echo "$out" | grep -q "$pattern"; then
        pass "$desc"
    else
        fail "$desc (exit=$actual_exit pattern='$pattern' not found in: $out)"
    fi
}

# ----- setup -----
mkdir -p "$TESTDIR"
echo "hello" > "$TESTDIR/a.txt"
echo "world" > "$TESTDIR/b.txt"
echo "log"   > "$TESTDIR/app.log"

echo "=== fm integration tests ==="
echo ""

# ---- 1. Basic baseline creation ----
echo "--- 1. Basic baseline creation ---"
check "baseline exits 0" 0 \
    "$FM" --baseline "$TESTDIR" -b "$BASELINE"

check "baseline file created" 0 \
    test -f "$BASELINE"

# ---- 2. No changes after baseline ----
echo "--- 2. No changes after baseline ---"
check "check exits 0 (no changes)" 0 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"

# ---- 3. Change detection ----
echo "--- 3. Change detection ---"
echo "modified" >> "$TESTDIR/a.txt"
check "check exits 2 (changes detected)" 2 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"

# ---- 4. Deleted file detection ----
echo "--- 4. Deleted file detection ---"
"$FM" --baseline "$TESTDIR" -b "$BASELINE" >/dev/null 2>&1
rm "$TESTDIR/b.txt"
check "check exits 2 (deleted file)" 2 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"
echo "world" > "$TESTDIR/b.txt"

# ---- 5. New file detection ----
echo "--- 5. New file detection ---"
"$FM" --baseline "$TESTDIR" -b "$BASELINE" >/dev/null 2>&1
echo "new" > "$TESTDIR/new.txt"
check "check exits 2 (new file)" 2 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"
rm "$TESTDIR/new.txt"
"$FM" --baseline "$TESTDIR" -b "$BASELINE" >/dev/null 2>&1

# ---- 6. Option order independence ----
echo "--- 6. Option order independence ---"
check "dir before --check" 0 \
    "$FM" "$TESTDIR" --check -b "$BASELINE"
check "--no-color before --check" 0 \
    "$FM" --no-color --check "$TESTDIR" -b "$BASELINE"
check "--check dir -b" 0 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"

# ---- 7. --exclude glob pattern ----
echo "--- 7. --exclude glob pattern ---"
"$FM" --baseline "$TESTDIR" -b "$BASELINE" >/dev/null 2>&1
# Modify the log file; with --exclude *.log it should still report 0
echo "modified log" >> "$TESTDIR/app.log"
check "--exclude *.log ignores log changes" 0 \
    "$FM" --check "$TESTDIR" -b "$BASELINE" --exclude "*.log"
# Without --exclude, changes are detected
check "without --exclude, log changes detected" 2 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"
# Restore
"$FM" --baseline "$TESTDIR" -b "$BASELINE" >/dev/null 2>&1

# ---- 8. Corrupt baseline ----
echo "--- 8. Corrupt baseline ---"
echo "CORRUPTED_DATA" > "$TMPDIR_BASE/corrupt.dat"
check_output "corrupt baseline gives error" 1 "invalid format" \
    "$FM" --check "$TESTDIR" -b "$TMPDIR_BASE/corrupt.dat"

# ---- 9. Mutually exclusive options ----
echo "--- 9. Mutually exclusive options ---"
check_output "--baseline --check errors" 1 "mutually exclusive" \
    "$FM" --baseline --check "$TESTDIR" -b "$BASELINE"
check_output "--check --reset errors" 1 "mutually exclusive" \
    "$FM" --check --reset -b "$BASELINE"

# ---- 10. No mode → usage ----
echo "--- 10. No mode → usage ---"
check "no mode exits 1" 1 \
    "$FM" "$TESTDIR" -b "$BASELINE"

# ---- 11. No target dir → error ----
echo "--- 11. No target dir → error ---"
check "no target dir exits 1" 1 \
    "$FM" --baseline -b "$BASELINE"

# ---- 12. Multiple baseline files ----
echo "--- 12. Multiple baseline files ---"
check "baseline to two files exits 0" 0 \
    "$FM" --baseline "$TESTDIR" -b "$BASELINE,$BASELINE2"
check "first baseline file exists" 0 test -f "$BASELINE"
check "second baseline file exists" 0 test -f "$BASELINE2"
check "check with first file exits 0" 0 \
    "$FM" --check "$TESTDIR" -b "$BASELINE"
check "check with second file exits 0" 0 \
    "$FM" --check "$TESTDIR" -b "$BASELINE2"

# ---- 13. --reset ----
echo "--- 13. --reset ---"
check "reset exits 0" 0 \
    "$FM" --reset -b "$BASELINE"
check "baseline file deleted after reset" 1 \
    test -f "$BASELINE"

# ---- 14. Unreadable file warning ----
echo "--- 14. Unreadable file warning ---"
"$FM" --baseline "$TESTDIR" -b "$BASELINE" >/dev/null 2>&1
chmod 000 "$TESTDIR/a.txt"
# unreadable file → warning on stderr; file appears deleted → exit 2 (changes)
check_output "unreadable file shows warning" 2 "Warning" \
    "$FM" --check "$TESTDIR" -b "$BASELINE"
chmod 644 "$TESTDIR/a.txt"

# ---- Summary ----
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
