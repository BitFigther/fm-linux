# File Monitor - File Change Detection Tool

## Overview

This C program detects changes in all files of your system, useful for verifying file integrity after middleware installation or system updates.

## Features

- **Fast processing**: Implemented in C for efficient scanning of large numbers of files
- **Strict verification**: Detects changes using MD5 hash
- **Lightweight**: Records only file size, modification time, and MD5 hash
- **Flexible**: Can target any directory

## Required Libraries

- OpenSSL (`libssl-dev` or `openssl-devel`)
  - Compatible with OpenSSL 1.1.x and 3.0.x

### For Ubuntu/Debian
```bash
sudo apt-get install libssl-dev
```

### For CentOS/RHEL
```bash
sudo yum install openssl-devel
# or
sudo dnf install openssl-devel
```

## Build

```bash
make
```

Or manually:
```bash
gcc -Wall -O2 -D_GNU_SOURCE -o fm fm.c -lssl -lcrypto
```

## Usage

### 1. Create Baseline
Before middleware installation, create a baseline:
```bash
./fm --baseline /
```

To target multiple directories (comma-separated or space-separated):
```bash
./fm --baseline /usr,/etc
./fm --baseline /usr /etc
```

### 2. Check for Changes
After installation, check for changes:
```bash
./fm --check /
```

To check multiple directories:
```bash
./fm --check /usr,/etc
```

### 3. Reset Baseline
```bash
./fm --reset
```

## Output Example

### When creating baseline
```
Creating baseline: /usr
Processing...
Baseline saved: 15432 files
```

### When changes are detected
```
Checking for changes: /usr
Baseline loaded: 15432 files (Created: Mon Sep  2 10:30:45 2025)
Processing...
Change detected: /usr/bin/example
  Modified time: 1693123845 -> 1693127445
  Size: 123456 -> 124000
  MD5 hash: abc123def456... -> def456abc123...
New file: /usr/lib/newfile.so (MD5: 789abc012def...)

=== Result ===
Changes detected: 2 file(s) changed
```

### When no changes
```
=== Result ===
No changes: No files were changed
```

## Exclude Patterns

You can exclude files/directories by pattern using the `--exclude` option. Multiple patterns can be specified with commas, and you can use `--exclude` multiple times:

```bash
./fm --baseline / --exclude /tmp/,/var/log/ --exclude /proc/
./fm --check /usr,/etc --exclude /tmp/
```

The following directories are automatically excluded:
- `/tmp/`
- `/var/log/`
- `/proc/`
- `/sys/`
- `/dev/`
## Usage

```
Usage:
  ./fm --baseline [directory(,directory...)] [--exclude path(,path...)] [--baseline-file path] : Create baseline (with MD5 hash)
  ./fm --check [directory(,directory...)] [--exclude path(,path...)] [--baseline-file path]    : Check for changes (strict MD5 check)
  ./fm --reset [--baseline-file path]                                                        : Reset baseline

Examples:
  ./fm --baseline /,/usr --exclude /tmp/,/var/log/ --baseline-file /tmp/mybase.dat     : Create baseline for / and /usr, excluding /tmp/, /var/log/, baseline file is /tmp/mybase.dat
  ./fm --check /etc,/opt --exclude /proc/ --baseline-file /tmp/mybase.dat              : Check for changes in /etc and /opt, excluding /proc/, using /tmp/mybase.dat
  ./fm --baseline /usr                                : Create baseline for /usr

Note: MD5 hash calculation may take time, but enables strict change detection.
      You can specify multiple directories and --exclude multiple times, each with a comma-separated list.
```
