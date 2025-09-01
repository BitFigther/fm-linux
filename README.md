# File Monitor - File Change Detection Tool (English)

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

To target a specific directory:
```bash
./fm --baseline /usr
./fm --baseline /etc
```

### 2. Check for Changes
After installation, check for changes:
```bash
./fm --check /
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

## Excluded Patterns

The following directories are automatically excluded:
- `/tmp/`
- `/var/log/`
- `/proc/`
- `/sys/`
- `/dev/`

## Exit Codes

- `0`: No changes
- `1`: Error
- `2`: Changes detected

## Notes

- Baseline file is saved as `/tmp/fm_baseline.dat`
- May take time for large systems
- MD5 hash calculation enables strict change detection but increases processing time
- Files without read permission are skipped automatically
- Works with OpenSSL 3.0 (uses EVP API)

## License

MIT License
