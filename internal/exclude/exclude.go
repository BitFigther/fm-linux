package exclude

import (
	"path/filepath"
	"strings"
)

var AutoPrefixes = []string{"/tmp/", "/var/log/", "/proc/", "/sys/", "/dev/"}

func IsAuto(path string) bool {
	for _, prefix := range AutoPrefixes {
		if strings.HasPrefix(path, prefix) {
			return true
		}
	}
	return false
}

func IsUser(path string, patterns []string) bool {
	base := filepath.Base(path)
	for _, pat := range patterns {
		if ok, _ := filepath.Match(pat, path); ok {
			return true
		}
		if ok, _ := filepath.Match(pat, base); ok {
			return true
		}
	}
	return false
}

func IsExcluded(path string, userPatterns []string) bool {
	return IsAuto(path) || IsUser(path, userPatterns)
}
