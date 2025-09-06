
# fm-linux

## Overview

- A C-language command-line tool for detecting file changes across the entire system.
- Designed for verifying differences after software installation or patch application.

## Features

- **Fast & Lightweight**: High-speed scanning of large directories with C implementation
- **Strict Change Detection**: Accurate detection of content changes using MD5 hash
- **Flexible Target Specification**: Support for arbitrary directories and multiple directories
- **Exclude Patterns**: Exclusion by partial matching of subdirectories or file names
- **Colored Output**: Color-coded display of changes (can be disabled with `--no-color`)

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
One of the following must be specified:

- `--baseline`, `-B` <directory(,directory...)>
  - Baseline creation mode. Records file information under specified directories.
  - Saved by default as `/tmp/fm_baseline.dat`.
  - Custom path can be specified with `--baseline-file`.

- `--check`, `-C` <directory(,directory...)>
  - Change check mode. Detects changed, new, and deleted files by comparing with baseline.
  - Custom baseline file can be specified with `--baseline-file`.

- `--reset` or `-R`  
  - Deletes (resets) the baseline file.
  - Custom baseline file can be specified with `--baseline-file`.

### Optional Options
- `--exclude <pattern>` or `-e <pattern>`  
  - Exclude patterns (partial match). Can be specified with commas or multiple times.
- `--baseline-file`, `-b` <filename>
  - Specify baseline file name.
- `--no-color`  
  - Disable colored output.

## Usage Examples

### 1. Create Baseline (Base file information before work)
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
Use `--exclude` for partial match exclusion. Can be specified with commas or multiple times.
```bash
fm -B / --exclude /tmp/,/var/log/ --exclude /proc/
fm -C /usr,/etc --exclude /log/,/tmp/
```

The following directories are automatically excluded:
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

## Output Examples

### When creating baseline
```
Creating baseline for: /Work/
Processing...
Create baseline file : /tmp/fm_baseline.dat
Baseline saved: 310 files
```

### When changes are detected
```
Checking for changes in: /Work/
Baseline loaded: 310 files (Created: Sun Sep  7 03:27:55 2025
)Processing...
Change detected: /Work/fm-linux/README.md
  Modified time: 20250907_032723 -> 20250907_032849
  Size: 3795 -> 3861
  MD5 hash: d7423ce7a38d70c8d9a11a8e0987dbbf -> c2d433f514cc82d9e7b47f84feeb6713
New file: /Work/fm-linux/duumy (MD5: d41d8cd98f00b204e9800998ecf8427e)
=== Result ===
Changes detected: 2 files
```

### When no changes
```
=== Result ===
No changes: No files were changed
```

## Notes and Limitations

- Maximum 16 baseline files can be specified. Excess files are ignored with warning.
- Exclude patterns use partial matching. Multiple specifications and multiple times are allowed.
- `--baseline-file` (`-b`) and `--exclude` (`-e`) options: **only the last specified value is effective (overwritten)**
- MD5 calculation is strict but increases processing time. May take time on large systems.
- Files that cannot be read are automatically skipped.
- Compatible with OpenSSL 3.0 (uses EVP API).
- Colored output can be disabled with `--no-color`.

## License

MIT License