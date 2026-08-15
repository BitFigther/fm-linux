package scanner

import (
	"crypto/md5"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"

	"fm/internal/baseline"
	"fm/internal/exclude"
)

// Options controls which files are scanned.
type Options struct {
	ExcludePatterns []string
}

// Modified holds the old and new state of a changed file.
type Modified struct {
	Path         string
	OldMtime     int64
	NewMtime     int64
	OldSize      int64
	NewSize      int64
	OldHash      [16]byte
	NewHash      [16]byte
	MtimeChanged bool
	SizeChanged  bool
	HashChanged  bool
}

// CheckResult is the structured output of a Check run.
type CheckResult struct {
	Modified   []Modified
	Added      []baseline.FileInfo
	Deleted    []string
	Unverified int
}

// TotalChanges returns the number of modified + added + deleted files.
func (r CheckResult) TotalChanges() int {
	return len(r.Modified) + len(r.Added) + len(r.Deleted)
}

// Baseline scans dirs and returns file metadata for all non-excluded regular files.
// unverified counts files that could not be read or hashed.
func Baseline(dirs []string, opts Options) (files []baseline.FileInfo, unverified int, err error) {
	err = walkFiles(dirs, func(path string, info fs.FileInfo) {
		if exclude.IsExcluded(path, opts.ExcludePatterns) {
			return
		}
		hash, err := calcMD5(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Warning: Cannot read file: %s (skipped)\n", path)
			unverified++
			return
		}
		files = append(files, baseline.FileInfo{
			Path:  path,
			Mtime: info.ModTime().Unix(),
			Size:  info.Size(),
			Hash:  hash,
		})
	})
	return
}

// Check compares the current state of dirs against bl and returns a CheckResult.
func Check(dirs []string, bl []baseline.FileInfo, opts Options) (CheckResult, error) {
	index := make(map[string]int, len(bl))
	for i, fi := range bl {
		index[fi.Path] = i
	}
	checked := make([]bool, len(bl))

	var result CheckResult

	err := walkFiles(dirs, func(path string, info fs.FileInfo) {
		if exclude.IsExcluded(path, opts.ExcludePatterns) {
			return
		}
		hash, err := calcMD5(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Warning: Cannot read file: %s (skipped)\n", path)
			result.Unverified++
			return
		}

		if i, ok := index[path]; ok {
			checked[i] = true
			ex := bl[i]
			m := Modified{
				Path:         path,
				OldMtime:     ex.Mtime, NewMtime: info.ModTime().Unix(),
				OldSize:      ex.Size, NewSize: info.Size(),
				OldHash:      ex.Hash, NewHash: hash,
				MtimeChanged: ex.Mtime != info.ModTime().Unix(),
				SizeChanged:  ex.Size != info.Size(),
				HashChanged:  ex.Hash != hash,
			}
			if m.MtimeChanged || m.SizeChanged || m.HashChanged {
				result.Modified = append(result.Modified, m)
			}
		} else {
			result.Added = append(result.Added, baseline.FileInfo{
				Path:  path,
				Mtime: info.ModTime().Unix(),
				Size:  info.Size(),
				Hash:  hash,
			})
		}
	})

	for i, fi := range bl {
		if !checked[i] && !exclude.IsUser(fi.Path, opts.ExcludePatterns) {
			result.Deleted = append(result.Deleted, fi.Path)
		}
	}

	return result, err
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

// walkFiles walks dirs and calls fn for each accessible regular file.
// Returns an error only when a root directory itself is inaccessible.
func walkFiles(dirs []string, fn func(path string, info fs.FileInfo)) error {
	for _, dir := range dirs {
		err := filepath.WalkDir(dir, func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				if path == dir {
					return err
				}
				fmt.Fprintf(os.Stderr, "Warning: Cannot access: %s (skipped)\n", path)
				return nil
			}
			if !d.Type().IsRegular() {
				return nil
			}
			info, err := d.Info()
			if err != nil {
				fmt.Fprintf(os.Stderr, "Warning: Cannot stat file: %s (skipped)\n", path)
				return nil
			}
			fn(path, info)
			return nil
		})
		if err != nil {
			return fmt.Errorf("directory scan error: %w", err)
		}
	}
	return nil
}
