package baseline_test

import (
	"os"
	"testing"

	"fm/internal/baseline"
)

func TestSaveLoadRoundtrip(t *testing.T) {
	original := []baseline.FileInfo{
		{Path: "/etc/passwd", Mtime: 1700000000, Size: 1024,
			Hash: [16]byte{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}},
		{Path: "/etc/hosts", Mtime: 1700000001, Size: 512,
			Hash: [16]byte{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}},
	}

	path := tempFile(t)
	if err := baseline.Save(path, original); err != nil {
		t.Fatalf("Save: %v", err)
	}

	got, ts, err := baseline.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if ts.IsZero() {
		t.Error("expected non-zero timestamp")
	}
	if len(got) != len(original) {
		t.Fatalf("got %d entries, want %d", len(got), len(original))
	}
	for i := range original {
		g, w := got[i], original[i]
		if g.Path != w.Path {
			t.Errorf("[%d] Path: got %q, want %q", i, g.Path, w.Path)
		}
		if g.Mtime != w.Mtime {
			t.Errorf("[%d] Mtime: got %d, want %d", i, g.Mtime, w.Mtime)
		}
		if g.Size != w.Size {
			t.Errorf("[%d] Size: got %d, want %d", i, g.Size, w.Size)
		}
		if g.Hash != w.Hash {
			t.Errorf("[%d] Hash mismatch: got %x, want %x", i, g.Hash, w.Hash)
		}
	}
}

func TestSaveLoadEmpty(t *testing.T) {
	path := tempFile(t)
	if err := baseline.Save(path, []baseline.FileInfo{}); err != nil {
		t.Fatalf("Save: %v", err)
	}
	got, _, err := baseline.Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if len(got) != 0 {
		t.Errorf("got %d entries, want 0", len(got))
	}
}

func TestLoadNotFound(t *testing.T) {
	_, _, err := baseline.Load("/non/existent/baseline.dat")
	if err == nil {
		t.Error("expected error for non-existent file")
	}
}

func TestLoadEmptyFile(t *testing.T) {
	_, _, err := baseline.Load(tempFile(t))
	if err == nil {
		t.Error("expected error for empty file")
	}
}

func TestLoadCorruptMagic(t *testing.T) {
	path := tempFile(t)
	os.WriteFile(path, []byte("CORRUPTED_DATA_NOT_FMBL"), 0644)
	_, _, err := baseline.Load(path)
	if err == nil {
		t.Error("expected error for corrupt magic")
	}
}

func TestLoadWrongVersion(t *testing.T) {
	path := tempFile(t)
	if err := baseline.Save(path, []baseline.FileInfo{{Path: "/test"}}); err != nil {
		t.Fatal(err)
	}
	// Patch bytes 4-7 (version field) to 99.
	data, _ := os.ReadFile(path)
	data[4], data[5], data[6], data[7] = 99, 0, 0, 0
	os.WriteFile(path, data, 0644)

	_, _, err := baseline.Load(path)
	if err == nil {
		t.Error("expected error for wrong version")
	}
}

func tempFile(t *testing.T) string {
	t.Helper()
	f, err := os.CreateTemp(t.TempDir(), "fm-baseline-*")
	if err != nil {
		t.Fatal(err)
	}
	f.Close()
	return f.Name()
}
