
# fm-linux

## Overview
- A command-line tool for detecting file modifications, written in Go.
- Designed for checking differences after software installation or patch application.

## Features
- **Strict Change Detection**: Accurate detection of content changes using MD5 hash.
- **Flexible Target Specification**: Supports arbitrary and multiple directories.
- **Exclude Patterns**: Exclude files by glob pattern (`filepath.Match`-based: supports `*.tmp`, `/var/log/*`, etc.).
- **Colored Output**: Color-coded display of changes (can be disabled with `--no-color`).

## Requirements
- Go 1.25 or later

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
go build -o build/fm .
sudo cp -f ./build/fm /usr/local/bin/
```

## Usage

```
fm [flags]
fm [command]
```

### Commands
- `baseline <directory>...`
  - Creates a baseline snapshot of the specified directories.
  - By default, saves to `/tmp/fm_baseline.dat`.
  - You can specify any path with `--baseline-file`.

- `check <directory>...`
  - Compares the current state of the specified directories against the baseline and reports changed, new, and deleted files.
  - You can specify any baseline file with `--baseline-file`.

- `reset`
  - Deletes the baseline file(s).
  - You can specify any baseline file with `--baseline-file`.
  - By default, deletes `/tmp/fm_baseline.dat`.

### Flags (inherited by all commands)
- `--exclude`, `-e` <pattern>
  - Exclude pattern using glob syntax (`path/filepath.Match`). Matched against the full path and basename.
    Examples: `*.log`, `/var/cache/*`, `*.tmp`
  - Can be specified as comma-separated or multiple times (all patterns are applied).
- `--baseline-file`, `-b` <path(,path...)>
  - Specify baseline file path(s), comma-separated.
- `--no-color`
  - Disable colored output.

See [docs/fm.md](docs/fm.md) for the full command reference.

## Usage Examples

### 1. Create Baseline (file info before work)
```bash
fm baseline /etc /usr
```

### 2. Check for Changes
```bash
fm check /etc /usr
```

### 3. Reset Baseline
```bash
fm reset
```

### Exclude Patterns
Use `--exclude` with glob patterns. Matched against the full path and basename.
Can be specified as comma-separated or multiple times (all are applied).
```bash
fm baseline /usr --exclude "*.log"
fm check /usr /etc --exclude "*.tmp,*.swp" --exclude "/var/cache/*"
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
- Exclude patterns use glob syntax (e.g., `*.log`, `/var/cache/*`). Multiple patterns can be specified comma-separated or with repeated `--exclude` flags.
- MD5 calculation is strict but increases processing time. May take time if there are many files.
- Files that cannot be read emit a warning to stderr and are counted as unverified. The exit code will be `1` if unverified files exist with no detected changes.
- Colored output can be disabled with `--no-color`.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
