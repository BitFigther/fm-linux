package scanner_test

import (
	"crypto/md5"
	"os"
	"path/filepath"
	"testing"

	"fm/internal/baseline"
	"fm/internal/scanner"
)

// mkTestDir creates a temp dir under the user's home to avoid auto-exclusion of /tmp/.
func mkTestDir(t *testing.T) string {
	t.Helper()
	home, err := os.UserHomeDir()
	if err != nil {
		t.Fatal(err)
	}
	dir, err := os.MkdirTemp(home, "fm-scantest-*")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.RemoveAll(dir) })
	return dir
}

func TestBaseline(t *testing.T) {
	dir := mkTestDir(t)
	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("file a"), 0644)
	os.WriteFile(filepath.Join(dir, "b.txt"), []byte("file b"), 0644)

	files, unverified, err := scanner.Baseline([]string{dir}, scanner.Options{})
	if err != nil {
		t.Fatalf("Baseline: %v", err)
	}
	if unverified != 0 {
		t.Errorf("unverified = %d, want 0", unverified)
	}
	if len(files) != 2 {
		t.Errorf("got %d files, want 2", len(files))
	}
}

func TestBaselineExclude(t *testing.T) {
	dir := mkTestDir(t)
	os.WriteFile(filepath.Join(dir, "keep.txt"), []byte("keep"), 0644)
	os.WriteFile(filepath.Join(dir, "drop.log"), []byte("drop"), 0644)

	opts := scanner.Options{ExcludePatterns: []string{"*.log"}}
	files, _, err := scanner.Baseline([]string{dir}, opts)
	if err != nil {
		t.Fatalf("Baseline: %v", err)
	}
	if len(files) != 1 || filepath.Base(files[0].Path) != "keep.txt" {
		t.Errorf("expected only keep.txt, got %d files", len(files))
	}
}

func TestBaselineNonExistentDir(t *testing.T) {
	_, _, err := scanner.Baseline([]string{"/non/existent/dir"}, scanner.Options{})
	if err == nil {
		t.Error("expected error for non-existent directory")
	}
}

func TestCheckNoChanges(t *testing.T) {
	dir := mkTestDir(t)
	content := []byte("unchanged")
	path := filepath.Join(dir, "file.txt")
	os.WriteFile(path, content, 0644)

	info, _ := os.Stat(path)
	bl := []baseline.FileInfo{{
		Path:  path,
		Mtime: info.ModTime().Unix(),
		Size:  info.Size(),
		Hash:  md5.Sum(content),
	}}

	result, err := scanner.Check([]string{dir}, bl, scanner.Options{})
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if result.TotalChanges() != 0 {
		t.Errorf("TotalChanges = %d, want 0", result.TotalChanges())
	}
}

func TestCheckDetectsModified(t *testing.T) {
	dir := mkTestDir(t)
	path := filepath.Join(dir, "file.txt")
	os.WriteFile(path, []byte("original"), 0644)

	info, _ := os.Stat(path)
	bl := []baseline.FileInfo{{
		Path:  path,
		Mtime: info.ModTime().Unix(),
		Size:  info.Size(),
		Hash:  md5.Sum([]byte("original")),
	}}

	os.WriteFile(path, []byte("modified content"), 0644)

	result, err := scanner.Check([]string{dir}, bl, scanner.Options{})
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if len(result.Modified) != 1 {
		t.Errorf("Modified = %d, want 1", len(result.Modified))
	}
}

func TestCheckDetectsAdded(t *testing.T) {
	dir := mkTestDir(t)
	os.WriteFile(filepath.Join(dir, "new.txt"), []byte("new"), 0644)

	result, err := scanner.Check([]string{dir}, nil, scanner.Options{})
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if len(result.Added) != 1 {
		t.Errorf("Added = %d, want 1", len(result.Added))
	}
}

func TestCheckDetectsDeleted(t *testing.T) {
	bl := []baseline.FileInfo{
		{Path: "/ghost/file.txt"},
	}
	result, err := scanner.Check([]string{mkTestDir(t)}, bl, scanner.Options{})
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if len(result.Deleted) != 1 {
		t.Errorf("Deleted = %d, want 1", len(result.Deleted))
	}
}

func TestCheckExcludeSkipsUserPattern(t *testing.T) {
	dir := mkTestDir(t)
	os.WriteFile(filepath.Join(dir, "app.log"), []byte("log"), 0644)

	opts := scanner.Options{ExcludePatterns: []string{"*.log"}}
	result, err := scanner.Check([]string{dir}, nil, opts)
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if len(result.Added) != 0 {
		t.Errorf("Added = %d, want 0 (*.log should be excluded)", len(result.Added))
	}
}
