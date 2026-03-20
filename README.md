

# fm-linux

## Overview
- A C-language command-line tool for detecting file modifications.
- Designed for checking differences after software installation or patch application.

## Features
- **Fast & Lightweight**: High-speed scanning of large directories with C implementation.
- **Strict Change Detection**: Accurate detection of content changes using MD5 hash.
- **Flexible Target Specification**: Supports arbitrary and multiple directories.
- **Exclude Patterns**: Exclude files by glob pattern (`fnmatch`-based: supports `*.tmp`, `/var/log/*`, etc.).
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
Install (explicit, requires sudo):
```bash
sudo make install
```
Or manually:
```bash
gcc -Wall -O2 -D_GNU_SOURCE -o build/fm fm.c -lssl -lcrypto
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
  - Exclude pattern using `fnmatch` glob syntax. Matched against the full path and basename.
    Examples: `*.log`, `/var/cache/*`, `*.tmp`
  - Can be specified as comma-separated or multiple times (all patterns are applied).
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
Use `--exclude` with glob patterns (`fnmatch`-based). Matched against the full path and basename.
Can be specified as comma-separated or multiple times (all are applied).
```bash
fm -B /usr --exclude "*.log"
fm -C /usr,/etc --exclude "*.tmp,*.swp" --exclude "/var/cache/*"
```
- The following directories are automatically excluded (regardless of `--exclude`):
  - `/tmp/`
  - `/var/log/`
  - `/proc/`
  - `/sys/`
  - `/dev/`

**Note**: Auto-excludes take priority. User `--exclude` patterns are evaluated after auto-excludes.

### Colored Output
Color-coded display by default. Disable with `--no-color`.

## Exit Codes
- `0`: No changes
- `1`: Error or some files could not be verified
- `2`: Changes detected

## Notes and Limitations
- Up to 8 baseline files can be specified. Any excess will be ignored with a warning.
- Exclude patterns use `fnmatch` glob syntax (e.g., `*.log`, `/var/cache/*`). Multiple patterns can be specified comma-separated or with repeated `--exclude` flags.
- MD5 calculation is strict but increases processing time. May take time if there are many files.
- Files that cannot be read emit a warning to stderr and are counted as unverified. The exit code will be `1` if unverified files exist with no detected changes.
- Compatible with OpenSSL 3.0 (uses EVP API).
- Colored output can be disabled with `--no-color`.
- Options and directories can be given in any order.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.