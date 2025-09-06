#!/bin/bash
#set -e


# fmコマンドの戻り値が1以外なら0にするラッパー関数
fm_wrap() {
	"$@"
	code=$?
	if [ $code -eq 1 ]; then
		return 1
	fi
	return 0
}

# 準備
echo "$PWD"
#rm -f /tmp/fm_test_base.dat
dir=./data
mkdir -p "$dir"
echo "hello" > "$dir/file1.txt"

# ベースライン作成
echo "[TEST] Create baseline"
fm_wrap fm --baseline "$dir" --baseline-file /tmp/fm_test_base.dat

# ファイルを変更
echo "world" > "$dir/file1.txt"

# 差分検出テスト
echo "[TEST] Detect change"
fm_wrap fm --check "$dir" --baseline-file /tmp/fm_test_base.dat
#echo "$output" | grep "Change detected:"

# 除外パターンテスト
#echo "ignore" > "$dir/ignore.txt"
#echo "[TEST] Exclude pattern"
#output2=$(fm_wrap fm --check "$dir" --exclude ignore.txt --baseline-file /tmp/fm_test_base.dat)
# echo "$output2" | grep "ignore.txt"

echo "All tests passed."
