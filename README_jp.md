# fm-linux

## 概要

ソフトウエア導入、パッチ適用などにシステム全体のファイルに変更があったかを高速に検出するためのlinuxコマンドです。


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
作業前にベースラインを作成:
```bash
./file_monitor --baseline /
```

複数ディレクトリをカンマ区切りまたはスペース区切りで指定可能:
```bash
./file_monitor --baseline /usr,/etc
./file_monitor --baseline /usr /etc
```

### 2. 変更チェック
ミドルウェア導入後に変更をチェック:
```bash
./file_monitor --check /
```

複数ディレクトリをチェック:
```bash
./file_monitor --check /usr,/etc
```

### 3. ベースラインリセット
```bash
./file_monitor --reset --baseline-file path
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
`--exclude` オプションで除外パターンを指定できます。カンマ区切りで複数指定や、`--exclude`の複数回指定も可能です:

```bash
./file_monitor --baseline / --exclude /tmp/,/var/log/ --exclude /proc/
./file_monitor --check /usr,/etc --exclude /tmp/
```

以下のディレクトリは自動的に除外されます:
- `/tmp/`
- `/var/log/`
- `/proc/`
- `/sys/`
- `/dev/`

```
Usage:
  ./file_monitor --baseline [directory(,directory...)] [--exclude path(,path...)] [--baseline-file path] : ベースライン作成 (MD5ハッシュ)
  ./file_monitor --check [directory(,directory...)] [--exclude path(,path...)] [--baseline-file path]    : 変更チェック (厳密なMD5チェック)
  ./file_monitor --reset [--baseline-file path]                                                        : ベースラインリセット

Examples:
  ./file_monitor --baseline /,/usr --exclude /tmp/,/var/log/ --baseline-file /tmp/mybase.dat     : /と/usrを対象、/tmp/・/var/log/を除外、ベースラインファイルは /tmp/mybase.dat
  ./file_monitor --check /etc,/opt --exclude /proc/ --baseline-file /tmp/mybase.dat              : /etcと/optを対象、/proc/を除外、/tmp/mybase.datを使用
  ./file_monitor --baseline /usr                                : /usrのみを対象

Note: MD5ハッシュ計算は厳密な変更検出が可能ですが、処理時間が増加します。
      ディレクトリ・--excludeはカンマ区切りや複数回指定が可能です。
```

## 終了コード

- `0`: 変更なし
- `1`: エラー
- `2`: 変更あり

## 注意事項

- ベースラインファイルはデフォルトで `/tmp/file_monitor_baseline_YYYYMMDD_HHMMSS.dat` のようにタイムスタンプ付きで保存されます。  
  `--baseline-file`（または `-b`）オプションで任意の保存先を指定することもできます。
- 大規模なシステムでは処理に時間がかかる場合があります
- MD5ハッシュ計算により、ファイル内容の厳密な変更検出が可能ですが、処理時間が増加します
- 読み取り権限のないファイルは自動的にスキップされます
- OpenSSL 3.0環境でも動作します（EVP APIを使用）

## ライセンス

MIT License
