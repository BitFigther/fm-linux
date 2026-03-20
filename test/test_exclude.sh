#!/bin/bash
# Regression tests for --exclude glob pattern matching (issue #14)

FM="$(dirname "$0")/../build/fm"
WORKDIR="$(mktemp -d "$(cd "$(dirname "$0")/.." && pwd)/fm_test_XXXXXX")"
BASEFILE="$WORKDIR/baseline.dat"
PASS=0
FAIL=0

cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

pass() { echo "  PASS: $1"; PASS=$((PASS+1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL+1)); }

run_check() {
    # Run fm --check; return its exit code without triggering set -e
    "$@" 2>&1 || true
}

# Prepare test files
mkdir -p "$WORKDIR/data" "$WORKDIR/logs"
echo "normal"  > "$WORKDIR/data/normal.txt"
echo "temp"    > "$WORKDIR/data/file.tmp"
echo "backup"  > "$WORKDIR/data/file.bak"
echo "log"     > "$WORKDIR/logs/app.log"
echo "sublog"  > "$WORKDIR/logs/sub.log"

echo "=== test_exclude.sh ==="

# -------------------------------------------------------
# Test 1: *.tmp excludes only .tmp files
# -------------------------------------------------------
echo "Test 1: --exclude '*.tmp' excludes .tmp files"
$FM -B "$WORKDIR/data" -b "$BASEFILE" -e "*.tmp" > /dev/null
echo "modified" > "$WORKDIR/data/file.tmp"
OUT=$(run_check $FM -C "$WORKDIR/data" -b "$BASEFILE" -e "*.tmp")
if echo "$OUT" | grep -q "file.tmp"; then
    fail "*.tmp file was detected despite being excluded"
else
    pass "*.tmp excluded correctly"
fi
echo "temp" > "$WORKDIR/data/file.tmp"
rm -f "$BASEFILE"

# -------------------------------------------------------
# Test 2: *.tmp does NOT suppress changes in .txt files
# -------------------------------------------------------
echo "Test 2: --exclude '*.tmp' does not suppress .txt changes"
$FM -B "$WORKDIR/data" -b "$BASEFILE" -e "*.tmp" > /dev/null
echo "changed" > "$WORKDIR/data/normal.txt"
OUT=$(run_check $FM -C "$WORKDIR/data" -b "$BASEFILE" -e "*.tmp")
if echo "$OUT" | grep -q "normal.txt"; then
    pass ".txt change detected correctly"
else
    fail ".txt change was not detected"
fi
echo "normal" > "$WORKDIR/data/normal.txt"
rm -f "$BASEFILE"

# -------------------------------------------------------
# Test 3: Full path glob pattern excludes all files in a dir
# -------------------------------------------------------
echo "Test 3: --exclude '$WORKDIR/logs/*' excludes files under logs/"
$FM -B "$WORKDIR/logs" -b "$BASEFILE" -e "$WORKDIR/logs/*" > /dev/null
echo "modified" > "$WORKDIR/logs/app.log"
OUT=$(run_check $FM -C "$WORKDIR/logs" -b "$BASEFILE" -e "$WORKDIR/logs/*")
if echo "$OUT" | grep -q "app.log"; then
    fail "app.log detected despite full-path glob exclusion"
else
    pass "full-path glob exclusion works"
fi
echo "log" > "$WORKDIR/logs/app.log"
rm -f "$BASEFILE"

# -------------------------------------------------------
# Test 4: Comma-separated patterns work
# -------------------------------------------------------
echo "Test 4: comma-separated --exclude '*.tmp,*.bak'"
$FM -B "$WORKDIR/data" -b "$BASEFILE" -e "*.tmp,*.bak" > /dev/null
echo "modified" > "$WORKDIR/data/file.tmp"
echo "modified" > "$WORKDIR/data/file.bak"
OUT=$(run_check $FM -C "$WORKDIR/data" -b "$BASEFILE" -e "*.tmp,*.bak")
if echo "$OUT" | grep -qE "file\.(tmp|bak)"; then
    fail "excluded files (*.tmp,*.bak) were detected"
else
    pass "comma-separated patterns excluded correctly"
fi
echo "temp" > "$WORKDIR/data/file.tmp"
echo "backup" > "$WORKDIR/data/file.bak"
rm -f "$BASEFILE"

# -------------------------------------------------------
# Test 5: No pattern → changes are detected
# -------------------------------------------------------
echo "Test 5: no --exclude → all changes detected"
$FM -B "$WORKDIR/data" -b "$BASEFILE" > /dev/null
echo "modified" > "$WORKDIR/data/file.tmp"
OUT=$(run_check $FM -C "$WORKDIR/data" -b "$BASEFILE")
if echo "$OUT" | grep -q "file.tmp"; then
    pass "change detected without exclusion"
else
    fail "change not detected without exclusion"
fi
rm -f "$BASEFILE"

# -------------------------------------------------------
# Summary
# -------------------------------------------------------
echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1

