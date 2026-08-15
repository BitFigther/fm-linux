package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

var testBin string

func TestMain(m *testing.M) {
	tmp, err := os.MkdirTemp("", "fm-testbin-*")
	if err != nil {
		panic("mktemp: " + err.Error())
	}
	defer os.RemoveAll(tmp)

	testBin = filepath.Join(tmp, "fm")
	if out, err := exec.Command("go", "build", "-o", testBin, ".").CombinedOutput(); err != nil {
		panic("build failed: " + string(out))
	}

	os.Exit(m.Run())
}

// run executes the test binary and returns exit code and combined output.
func run(args ...string) (int, string) {
	cmd := exec.Command(testBin, args...)
	out, _ := cmd.CombinedOutput()
	return cmd.ProcessState.ExitCode(), string(out)
}

// mkTestDir creates a temp directory outside /tmp/ so it is not auto-excluded by fm.
func mkTestDir(t *testing.T) string {
	t.Helper()
	home, err := os.UserHomeDir()
	if err != nil {
		t.Fatal(err)
	}
	dir, err := os.MkdirTemp(home, "fm-inttest-*")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.RemoveAll(dir) })
	return dir
}

func TestIntegrationBasicWorkflow(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")
	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("hello"), 0644)

	if code, _ := run("baseline", dir, "-b", bl); code != 0 {
		t.Fatalf("baseline: exit %d", code)
	}
	if code, _ := run("check", dir, "-b", bl); code != 0 {
		t.Fatalf("check (no changes): exit %d", code)
	}
}

func TestIntegrationChangeDetection(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")
	path := filepath.Join(dir, "a.txt")

	os.WriteFile(path, []byte("original"), 0644)
	run("baseline", dir, "-b", bl)

	os.WriteFile(path, []byte("modified"), 0644)
	if code, _ := run("check", dir, "-b", bl); code != 2 {
		t.Errorf("check (modified): exit %d, want 2", code)
	}
}

func TestIntegrationDeletedFile(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")
	path := filepath.Join(dir, "a.txt")

	os.WriteFile(path, []byte("hello"), 0644)
	run("baseline", dir, "-b", bl)
	os.Remove(path)

	if code, _ := run("check", dir, "-b", bl); code != 2 {
		t.Errorf("check (deleted): exit %d, want 2", code)
	}
}

func TestIntegrationNewFile(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")

	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("hello"), 0644)
	run("baseline", dir, "-b", bl)
	os.WriteFile(filepath.Join(dir, "new.txt"), []byte("new"), 0644)

	if code, _ := run("check", dir, "-b", bl); code != 2 {
		t.Errorf("check (new file): exit %d, want 2", code)
	}
}

func TestIntegrationExclude(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")

	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("hello"), 0644)
	os.WriteFile(filepath.Join(dir, "b.log"), []byte("log"), 0644)
	run("baseline", dir, "-b", bl, "--exclude", "*.log")

	os.WriteFile(filepath.Join(dir, "b.log"), []byte("changed"), 0644)
	if code, _ := run("check", dir, "-b", bl, "--exclude", "*.log"); code != 0 {
		t.Errorf("check with --exclude: exit %d, want 0", code)
	}
}

func TestIntegrationReset(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")

	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("hello"), 0644)
	run("baseline", dir, "-b", bl)

	if code, _ := run("reset", "-b", bl); code != 0 {
		t.Errorf("reset: exit %d, want 0", code)
	}
	if _, err := os.Stat(bl); !os.IsNotExist(err) {
		t.Error("baseline file should not exist after reset")
	}
}

func TestIntegrationCorruptBaseline(t *testing.T) {
	dir := t.TempDir()
	bl := filepath.Join(t.TempDir(), "corrupt.dat")
	os.WriteFile(bl, []byte("CORRUPTED"), 0644)

	code, out := run("check", dir, "-b", bl)
	if code != 1 {
		t.Errorf("corrupt baseline: exit %d, want 1", code)
	}
	if !strings.Contains(out, "invalid format") {
		t.Errorf("expected 'invalid format' in output, got: %s", out)
	}
}

func TestIntegrationMultipleBaselineFiles(t *testing.T) {
	dir := mkTestDir(t)
	tmp := mkTestDir(t)
	bl1 := filepath.Join(tmp, "b1.dat")
	bl2 := filepath.Join(tmp, "b2.dat")

	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("hello"), 0644)
	if code, _ := run("baseline", dir, "-b", bl1+","+bl2); code != 0 {
		t.Fatalf("baseline to two files: exit %d", code)
	}
	if _, err := os.Stat(bl1); err != nil {
		t.Error("first baseline file not created")
	}
	if _, err := os.Stat(bl2); err != nil {
		t.Error("second baseline file not created")
	}
	if code, _ := run("check", dir, "-b", bl1); code != 0 {
		t.Errorf("check with bl1: exit %d, want 0", code)
	}
}

func TestIntegrationNoColor(t *testing.T) {
	dir := mkTestDir(t)
	bl := filepath.Join(mkTestDir(t), "baseline.dat")

	os.WriteFile(filepath.Join(dir, "a.txt"), []byte("hello"), 0644)
	run("baseline", dir, "-b", bl)
	os.WriteFile(filepath.Join(dir, "new.txt"), []byte("new"), 0644)

	_, out := run("--no-color", "check", dir, "-b", bl)
	if strings.Contains(out, "\033[") {
		t.Error("--no-color output should not contain ANSI escape codes")
	}
}

func TestIntegrationUnknownSubcommand(t *testing.T) {
	if code, _ := run("unknown-cmd"); code != 1 {
		t.Errorf("unknown subcommand: exit %d, want 1", code)
	}
}

func TestIntegrationNoSubcommand(t *testing.T) {
	if code, _ := run(); code != 0 {
		t.Errorf("no subcommand: exit %d, want 0 (shows help)", code)
	}
}

func TestIntegrationMissingTargetDir(t *testing.T) {
	bl := filepath.Join(t.TempDir(), "baseline.dat")
	if code, _ := run("baseline", "-b", bl); code != 1 {
		t.Errorf("baseline no args: exit %d, want 1", code)
	}
	if code, _ := run("check", "-b", bl); code != 1 {
		t.Errorf("check no args: exit %d, want 1", code)
	}
}
