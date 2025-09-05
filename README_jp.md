# File Monitor - システムファイル変更検出ツール

## 概要

ミドルウェア導入後などにシステム全体のファイルに変更があったかを高速に検出するためのC言語プログラムです。

## 特徴

- **高速処理**: C言語で実装され、大量のファイルを効率的にスキャン
- **厳密な検証**: MD5ハッシュによる内容の変更検出
- **軽量**: ファイルサイズ、修正時間、MD5ハッシュのみを記録
- **柔軟性**: 任意のディレクトリを対象にできる

## 必要なライブラリ

- OpenSSL (`libssl-dev`または`openssl-devel`)
  - OpenSSL 1.1.x および 3.0.x に対応

### Ubuntu/Debianの場合
```bash
sudo apt-get install libssl-dev
```

### CentOS/RHELの場合
```bash
sudo yum install openssl-devel
# または
sudo dnf install openssl-devel
```

## コンパイル

```bash
make
```

または手動で:
```bash
gcc -Wall -O2 -D_GNU_SOURCE -o file_monitor file_monitor.c -lssl -lcrypto
```

## 使用方法

### 1. ベースライン作成
ミドルウェア導入前にベースラインを作成:
```bash
./file_monitor --baseline /
```

特定のディレクトリのみを対象にする場合:
```bash
./file_monitor --baseline /usr
./file_monitor --baseline /etc
```

### 2. 変更チェック
ミドルウェア導入後に変更をチェック:
```bash
./file_monitor --check /
```

### 3. ベースラインリセット
```bash
./file_monitor --reset
```

## 出力例

### ベースライン作成時
```
ベースライン作成中: /usr
処理中...
ベースライン保存完了: 15432 ファイル
```

### 変更検出時
```
変更チェック中: /usr
ベースライン読み込み完了: 15432 ファイル (作成日時: Mon Sep  2 10:30:45 2025)
処理中...
変更検出: /usr/bin/example
  修正時間: 1693123845 -> 1693127445
  サイズ: 123456 -> 124000
  MD5ハッシュ: abc123def456... -> def456abc123...
新規ファイル: /usr/lib/newfile.so (MD5: 789abc012def...)

=== 結果 ===
変更検出: 2 件のファイルに変更があります
```

### 変更なしの場合
```
=== 結果 ===
変更なし: ファイルに変更はありませんでした
```

## 除外パターン

以下のディレクトリは自動的に除外されます:
- `/tmp/`
- `/var/log/`
- `/proc/`
- `/sys/`
- `/dev/`

## 終了コード

- `0`: 変更なし
- `1`: エラー
- `2`: 変更あり

## 注意事項

- ベースラインファイルは `/tmp/file_monitor_baseline.dat` に保存されます
- 大規模なシステムでは処理に時間がかかる場合があります
- MD5ハッシュ計算により、ファイル内容の厳密な変更検出が可能ですが、処理時間が増加します
- 読み取り権限のないファイルは自動的にスキップされます
- OpenSSL 3.0環境でも動作します（EVP APIを使用）

## ライセンス

MIT License
