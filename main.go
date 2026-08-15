/*
 * Copyright (c) 2025 BitFigther
 * Licensed under the MIT License
 */

package main

import (
	"crypto/md5"
	"encoding/binary"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"time"
)

const (
	baselineMagic    = "FMBL"
	baselineVersion  = uint32(2) // version 2: fixed-size types (portable)
	defaultBaseline  = "/tmp/fm_baseline.dat"
	maxBaselineFiles = 8
)

var useColor = true

func cRed() string    { if useColor { return "\033[31m" }; return "" }
func cGreen() string  { if useColor { return "\033[32m" }; return "" }
func cYellow() string { if useColor { return "\033[33m" }; return "" }
func cReset() string  { if useColor { return "\033[0m" }; return "" }

type fileInfo struct {
	path  string
	mtime int64
	size  int64
	hash  [16]byte
}

var (
	baseline        []fileInfo
	excludePatterns []string
	fileChecked     []bool
	changesDetected int
	unverifiedFiles int
)

var autoExcluded = []string{"/tmp/", "/var/log/", "/proc/", "/sys/", "/dev/"}

func isAutoExcluded(path string) bool {
	for _, prefix := range autoExcluded {
		if strings.HasPrefix(path, prefix) {
			return true
		}
	}
	return false
}

func isUserExcluded(path string) bool {
	base := filepath.Base(path)
	for _, pat := range excludePatterns {
		if ok, _ := filepath.Match(pat, path); ok {
			return true
		}
		if ok, _ := filepath.Match(pat, base); ok {
			return true
		}
	}
	return false
}

func calcMD5(path string) ([16]byte, error) {
	var result [16]byte
	f, err := os.Open(path)
	if err != nil {
		return result, err
	}
	defer f.Close()
	h := md5.New()
	if _, err := io.Copy(h, f); err != nil {
		return result, err
	}
	copy(result[:], h.Sum(nil))
	return result, nil
}

// walkFiles walks dirs and calls fn for each regular file. Returns the first
// hard error (e.g. root directory not found); per-file errors are warnings.
func walkFiles(dirs []string, fn func(path string, info fs.FileInfo)) error {
	for _, dir := range dirs {
		err := filepath.WalkDir(dir, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				if path == dir {
					return err // root missing → hard error
				}
				fmt.Fprintf(os.Stderr, "Warning: Cannot access: %s (skipped)\n", path)
				unverifiedFiles++
				return nil
			}
			if !d.Type().IsRegular() {
				return nil
			}
			info, err := d.Info()
			if err != nil {
				fmt.Fprintf(os.Stderr, "Warning: Cannot stat file: %s (skipped)\n", path)
				unverifiedFiles++
				return nil
			}
			fn(path, info)
			return nil
		})
		if err != nil {
			fmt.Fprintf(os.Stderr, "Directory scan error: %v\n", err)
			return err
		}
	}
	return nil
}

func runBaseline(dirs []string) error {
	return walkFiles(dirs, func(path string, info fs.FileInfo) {
		if isAutoExcluded(path) || isUserExcluded(path) {
			return
		}
		hash, err := calcMD5(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Warning: Cannot read file: %s (skipped)\n", path)
			unverifiedFiles++
			return
		}
		baseline = append(baseline, fileInfo{
			path:  path,
			mtime: info.ModTime().Unix(),
			size:  info.Size(),
			hash:  hash,
		})
	})
}

func saveBaseline(paths []string) error {
	var lastErr error
	for _, p := range paths {
		if err := saveBaselineTo(p); err != nil {
			fmt.Fprintf(os.Stderr, "Error: Failed to write baseline file: %s\n", p)
			lastErr = err
		}
	}
	return lastErr
}

func saveBaselineTo(path string) error {
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	w := func(v any) error { return binary.Write(f, binary.LittleEndian, v) }

	if _, err := f.WriteString(baselineMagic); err != nil {
		return err
	}
	if err := w(baselineVersion); err != nil {
		return err
	}
	if err := w(int64(time.Now().Unix())); err != nil {
		return err
	}
	if err := w(int32(len(baseline))); err != nil {
		return err
	}
	for _, fi := range baseline {
		pb := []byte(fi.path)
		if err := w(int32(len(pb))); err != nil {
			return err
		}
		if _, err := f.Write(pb); err != nil {
			return err
		}
		if err := w(fi.mtime); err != nil {
			return err
		}
		if err := w(fi.size); err != nil {
			return err
		}
		if _, err := f.Write(fi.hash[:]); err != nil {
			return err
		}
	}

	fmt.Printf("Create baseline file : %s \n", path)
	fmt.Printf("Baseline saved: %d files\n", len(baseline))
	return nil
}

func loadBaseline(paths []string) bool {
	for _, p := range paths {
		if loadBaselineFrom(p) {
			return true
		}
	}
	return false
}

func loadBaselineFrom(path string) bool {
	f, err := os.Open(path)
	if err != nil {
		return false
	}
	defer f.Close()

	magic := make([]byte, 4)
	if _, err := io.ReadFull(f, magic); err != nil || string(magic) != baselineMagic {
		fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' has invalid format (bad magic). "+
			"Please recreate it with --baseline.\n", path)
		return false
	}

	var version uint32
	if err := binary.Read(f, binary.LittleEndian, &version); err != nil {
		fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is truncated.\n", path)
		return false
	}
	if version != baselineVersion {
		fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' has unsupported version %d (expected %d). "+
			"Please recreate it with --baseline.\n", path, version, baselineVersion)
		return false
	}

	var ts int64
	var count int32
	if err := binary.Read(f, binary.LittleEndian, &ts); err != nil {
		fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted (truncated header).\n", path)
		return false
	}
	if err := binary.Read(f, binary.LittleEndian, &count); err != nil {
		fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted (truncated header).\n", path)
		return false
	}

	entries := make([]fileInfo, 0, count)
	for i := int32(0); i < count; i++ {
		var pathLen int32
		if err := binary.Read(f, binary.LittleEndian, &pathLen); err != nil || pathLen <= 0 || pathLen > 4096 {
			fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted.\n", path)
			return false
		}
		pb := make([]byte, pathLen)
		if _, err := io.ReadFull(f, pb); err != nil {
			fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted.\n", path)
			return false
		}
		var fi fileInfo
		fi.path = string(pb)
		if err := binary.Read(f, binary.LittleEndian, &fi.mtime); err != nil {
			fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted.\n", path)
			return false
		}
		if err := binary.Read(f, binary.LittleEndian, &fi.size); err != nil {
			fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted.\n", path)
			return false
		}
		if _, err := io.ReadFull(f, fi.hash[:]); err != nil {
			fmt.Fprintf(os.Stderr, "Error: Baseline file '%s' is corrupted.\n", path)
			return false
		}
		entries = append(entries, fi)
	}

	baseline = entries
	fileChecked = make([]bool, len(baseline))

	fmt.Printf("Baseline loaded: %d files (Created: %s)\n",
		len(baseline), time.Unix(ts, 0).Format("2006-01-02 15:04:05"))
	return true
}

func runCheck(dirs []string) error {
	index := make(map[string]int, len(baseline))
	for i, fi := range baseline {
		index[fi.path] = i
	}

	return walkFiles(dirs, func(path string, info fs.FileInfo) {
		if isAutoExcluded(path) || isUserExcluded(path) {
			return
		}
		hash, err := calcMD5(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Warning: Cannot read file: %s (skipped)\n", path)
			unverifiedFiles++
			return
		}

		if i, ok := index[path]; ok {
			fileChecked[i] = true
			ex := baseline[i]
			hashChanged := ex.hash != hash
			mtimeChanged := ex.mtime != info.ModTime().Unix()
			sizeChanged := ex.size != info.Size()
			if hashChanged || mtimeChanged || sizeChanged {
				fmt.Printf("%sChange detected: %s%s\n", cYellow(), path, cReset())
				if mtimeChanged {
					fmt.Printf("  Modified time: %s -> %s\n",
						time.Unix(ex.mtime, 0).Format("20060102_150405"),
						info.ModTime().Format("20060102_150405"))
				}
				if sizeChanged {
					fmt.Printf("  Size: %d -> %d\n", ex.size, info.Size())
				}
				if hashChanged {
					fmt.Printf("  MD5 hash: %x -> %x\n", ex.hash, hash)
				}
				changesDetected++
			}
		} else {
			fmt.Printf("%sNew file: %s (MD5: %x)%s\n", cGreen(), path, hash, cReset())
			changesDetected++
		}
	})
}

func reportDeleted() {
	for i, fi := range baseline {
		if fileChecked[i] || isUserExcluded(fi.path) {
			continue
		}
		fmt.Printf("%sDeleted file: %s%s\n", cRed(), fi.path, cReset())
		changesDetected++
	}
}

func printUsage(prog string) {
	fmt.Printf("Usage:\n")
	fmt.Printf("  %s --baseline [directory...] [options] : Create baseline (with MD5 hash)\n", prog)
	fmt.Printf("  %s --check    [directory...] [options] : Check for changes (strict MD5 check)\n", prog)
	fmt.Printf("  %s --reset    [options]                : Reset baseline\n", prog)
	fmt.Printf("\nRequired options (choose exactly one):\n")
	fmt.Printf("  --baseline, -B    Create baseline\n")
	fmt.Printf("  --check,    -C    Check for changes\n")
	fmt.Printf("  --reset,    -R    Reset (delete) baseline file\n")
	fmt.Printf("\nOptional options:\n")
	fmt.Printf("  --exclude, -e <path(,path...)>           Exclude path(s) from scan\n")
	fmt.Printf("  --baseline-file, -b <path(,path...)>     Specify baseline file path(s)\n")
	fmt.Printf("  --no-color                               Disable colored output\n")
	fmt.Printf("\nNote: Options and directories can appear in any order.\n")
	fmt.Printf("      --exclude/-e may be specified multiple times.\n")
}

func splitComma(s string) []string {
	parts := strings.Split(s, ",")
	out := parts[:0]
	for _, p := range parts {
		if p = strings.TrimSpace(p); p != "" {
			out = append(out, p)
		}
	}
	return out
}

func main() {
	prog := os.Args[0]
	args := os.Args[1:]

	var (
		mode              byte // 'B', 'C', 'R'
		baselineFilePaths []string
		baselineExplicit  bool
		targetDirs        []string
	)

	for i := 0; i < len(args); i++ {
		arg := args[i]
		nextArg := func() (string, bool) {
			i++
			if i >= len(args) {
				return "", false
			}
			return args[i], true
		}

		switch arg {
		case "--baseline", "-B":
			if mode != 0 {
				fmt.Fprintln(os.Stderr, "Error: --baseline, --check, and --reset are mutually exclusive.")
				os.Exit(1)
			}
			mode = 'B'
		case "--check", "-C":
			if mode != 0 {
				fmt.Fprintln(os.Stderr, "Error: --baseline, --check, and --reset are mutually exclusive.")
				os.Exit(1)
			}
			mode = 'C'
		case "--reset", "-R":
			if mode != 0 {
				fmt.Fprintln(os.Stderr, "Error: --baseline, --check, and --reset are mutually exclusive.")
				os.Exit(1)
			}
			mode = 'R'
		case "--no-color":
			useColor = false
		case "--exclude", "-e":
			v, ok := nextArg()
			if !ok {
				fmt.Fprintf(os.Stderr, "Error: %s requires an argument.\n", arg)
				os.Exit(1)
			}
			excludePatterns = append(excludePatterns, splitComma(v)...)
		case "--baseline-file", "-b":
			v, ok := nextArg()
			if !ok {
				fmt.Fprintf(os.Stderr, "Error: %s requires an argument.\n", arg)
				os.Exit(1)
			}
			paths := splitComma(v)
			if len(paths) > maxBaselineFiles {
				fmt.Fprintf(os.Stderr, "Warning: Too many baseline files. Only the first %d will be used.\n", maxBaselineFiles)
				paths = paths[:maxBaselineFiles]
			}
			baselineFilePaths = paths
			baselineExplicit = true
		default:
			if strings.HasPrefix(arg, "-") {
				fmt.Fprintf(os.Stderr, "Unknown option: %s\n", arg)
				printUsage(prog)
				os.Exit(1)
			}
			targetDirs = append(targetDirs, splitComma(arg)...)
		}
	}

	if !baselineExplicit {
		baselineFilePaths = []string{defaultBaseline}
	}

	if mode == 0 {
		printUsage(prog)
		os.Exit(1)
	}

	exitCode := 0

	switch mode {
	case 'R':
		for _, p := range baselineFilePaths {
			if err := os.Remove(p); err != nil {
				fmt.Fprintf(os.Stderr, "Baseline file not found: %s\n", p)
				exitCode = 1
			} else {
				fmt.Printf("Baseline file deleted: %s\n", p)
			}
		}

	case 'B':
		if len(targetDirs) == 0 {
			fmt.Fprintln(os.Stderr, "Error: No target directory specified.")
			printUsage(prog)
			os.Exit(1)
		}
		fmt.Print("Creating baseline for:")
		for _, d := range targetDirs {
			fmt.Printf(" %s", d)
		}
		fmt.Println("\nProcessing...")
		if err := runBaseline(targetDirs); err != nil {
			exitCode = 1
		} else {
			if unverifiedFiles > 0 {
				fmt.Fprintf(os.Stderr, "Warning: %d file(s) could not be read and were excluded from the baseline.\n", unverifiedFiles)
			}
			if err := saveBaseline(baselineFilePaths); err != nil {
				exitCode = 1
			}
		}

	case 'C':
		if len(targetDirs) == 0 {
			fmt.Fprintln(os.Stderr, "Error: No target directory specified.")
			printUsage(prog)
			os.Exit(1)
		}
		fmt.Print("Checking for changes in:")
		for _, d := range targetDirs {
			fmt.Printf(" %s", d)
		}
		fmt.Println()
		if !loadBaseline(baselineFilePaths) {
			fmt.Println("Error: Baseline file not found.")
			fmt.Println("Please create a baseline first using --baseline or -B option.")
			exitCode = 1
		} else {
			fmt.Println("Processing...")
			if err := runCheck(targetDirs); err != nil {
				exitCode = 1
			} else {
				reportDeleted()
				fmt.Println("\n=== Result ===")
				if unverifiedFiles > 0 {
					fmt.Fprintf(os.Stderr, "Warning: %d file(s) could not be verified (read error or hash failure).\n", unverifiedFiles)
				}
				switch {
				case changesDetected > 0:
					fmt.Printf("Changes detected: %d file(s) changed\n", changesDetected)
					exitCode = 2
				case unverifiedFiles > 0:
					fmt.Printf("No changes confirmed, but %d file(s) could not be verified.\n", unverifiedFiles)
					exitCode = 1
				default:
					fmt.Println("No changes: No files were changed")
				}
			}
		}
	}

	os.Exit(exitCode)
}
