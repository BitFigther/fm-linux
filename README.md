

# fm-linux

## Overview
- A C-language command-line tool for detecting file modifications.
- Designed for checking differences after software installation or patch application.

## Features
- **Fast & Lightweight**: High-speed scanning of large directories with C implementation.
- **Strict Change Detection**: Accurate detection of content changes using MD5 hash.
- **Flexible Target Specification**: Supports arbitrary and multiple directories.
- **Exclude Patterns**: Exclude by partial match of subdirectory or file name.
- **Colored Output**: Color-coded display of changes (can be disabled with `--no-color`).

## Required Libraries
- OpenSSL (`libssl-dev` or `openssl-devel`)
  - Compatible with OpenSSL 1.1.x / 3.0.x

### Ubuntu/Debian
```bash
sudo apt-get install libssl-dev
```

### CentOS/RHEL
```bash
sudo dnf install openssl-devel
```

## Build
```bash
make
```
Or manually:
```bash
gcc -Wall -O2 -D_GNU_SOURCE -o fm fm.c -lssl -lcrypto
sudo cp -f ./build/fm /usr/local/bin/
```

## Arguments

### Required Options
Specify one of the following:

- `--baseline`, `-B` <directory(,directory...)>
  - Baseline creation mode. Records file information under the specified directories.
  - By default, saves to `/tmp/fm_baseline.dat`.
  - You can specify any path with `--baseline-file`.

- `--check`, `-C` <directory(,directory...)>
  - Change check mode. Detects changed, new, and deleted files by comparing with the baseline.
  - You can specify any baseline file with `--baseline-file`.

- `--reset` or `-R`
  - Deletes (resets) the baseline file.
  - You can specify any baseline file with `--baseline-file`.
  - By default, deletes `/tmp/fm_baseline.dat`.

### Optional Options
- `--exclude <pattern>` or `-e <pattern>`
  - Exclude pattern (partial match). Can be specified as comma-separated or multiple times.
- `--baseline-file`, `-b` <filename>
  - Specify baseline file name.
- `--no-color`
  - Disable colored output.

## Usage Examples

### 1. Create Baseline (file info before work)
```bash
fm -B /etc,/usr
```

### 2. Check for Changes
```bash
fm -C /etc,/usr
```

### 3. Reset Baseline
```bash
fm -R
```

### Exclude Patterns
Use `--exclude` for partial match exclusion. Can be specified as comma-separated or multiple times.
```bash
fm -B / --exclude /tmp/,/var/log/
fm -C /usr,/etc --exclude /log/,/tmp/
```
- The following directories are automatically excluded:
  - `/tmp/`
  - `/var/log/`
  - `/proc/`
  - `/sys/`
  - `/dev/`

### Colored Output
Color-coded display by default. Disable with `--no-color`.

## Exit Codes
- `0`: No changes
- `1`: Error
- `2`: Changes detected

## Notes and Limitations
- Up to 8 baseline files can be specified. Any excess will be ignored with a warning.
- Exclude patterns use partial match. Multiple specifications and multiple times are allowed.
- If you specify `--baseline-file` (`-b`) or `--exclude` (`-e`) multiple times, **only the last value is effective (overwritten)**.
- MD5 calculation is strict but increases processing time. May take time if there are many files.
- Files that cannot be read are automatically skipped.
- Compatible with OpenSSL 3.0 (uses EVP API).
- Colored output can be disabled with `--no-color`.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.