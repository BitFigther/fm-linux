package exclude_test

import (
	"testing"

	"fm/internal/exclude"
)

func TestIsAuto(t *testing.T) {
	tests := []struct {
		path string
		want bool
	}{
		{"/tmp/file.txt", true},
		{"/var/log/syslog", true},
		{"/proc/cpuinfo", true},
		{"/sys/kernel/mm", true},
		{"/dev/null", true},
		{"/etc/passwd", false},
		{"/usr/bin/ls", false},
		{"/home/user/file.txt", false},
		{"/tmpfile.txt", false}, // /tmp/ prefix required
	}
	for _, tt := range tests {
		t.Run(tt.path, func(t *testing.T) {
			if got := exclude.IsAuto(tt.path); got != tt.want {
				t.Errorf("IsAuto(%q) = %v, want %v", tt.path, got, tt.want)
			}
		})
	}
}

func TestIsUser(t *testing.T) {
	tests := []struct {
		name     string
		patterns []string
		path     string
		want     bool
	}{
		{
			name:     "*.log matches by basename",
			patterns: []string{"*.log"},
			path:     "/var/app/server.log",
			want:     true,
		},
		{
			name:     "*.log does not match .txt",
			patterns: []string{"*.log"},
			path:     "/var/app/server.txt",
			want:     false,
		},
		{
			// filepath.Match では * が / を超えないので直下のみマッチ
			name:     "glob matches direct child",
			patterns: []string{"/var/cache/*"},
			path:     "/var/cache/lockfile",
			want:     true,
		},
		{
			name:     "glob does not match nested path",
			patterns: []string{"/var/cache/*"},
			path:     "/var/cache/apt/lock",
			want:     false,
		},
		{
			name:     "no patterns excludes nothing",
			patterns: nil,
			path:     "/etc/passwd",
			want:     false,
		},
		{
			name:     "first of multiple patterns matches",
			patterns: []string{"*.log", "*.tmp"},
			path:     "/app/data.log",
			want:     true,
		},
		{
			name:     "second of multiple patterns matches",
			patterns: []string{"*.log", "*.tmp"},
			path:     "/app/data.tmp",
			want:     true,
		},
		{
			name:     "none of multiple patterns match",
			patterns: []string{"*.log", "*.tmp"},
			path:     "/app/data.txt",
			want:     false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := exclude.IsUser(tt.path, tt.patterns); got != tt.want {
				t.Errorf("IsUser(%q, %v) = %v, want %v", tt.path, tt.patterns, got, tt.want)
			}
		})
	}
}
