#!/bin/bash
set -e

# 準備
echo "$PWD"
rm -f /tmp/fm_test_base.dat
dir=test/data
mkdir -p "$dir"
echo "hello" > "$dir/file1.txt"

# ベースライン作成
echo "[TEST] Create baseline"
../fm --baseline "$dir" --baseline-file /tmp/fm_test_base.dat

# ファイルを変更
echo "world" > "$dir/file1.txt"

# 差分検出テスト
echo "[TEST] Detect change"
output=$(../fm --check "$dir" --baseline-file /tmp/fm_test_base.dat)
echo "$output" | grep "Change detected:"

# 除外パターンテスト
echo "ignore" > "$dir/ignore.txt"
echo "[TEST] Exclude pattern"
output2=$(../fm --check "$dir" --exclude ignore.txt --baseline-file /tmp/fm_test_base.dat)
! echo "$output2" | grep "ignore.txt"

echo "All tests passed."
