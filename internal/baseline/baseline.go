package baseline

import (
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"time"
)

const (
	magic   = "FMBL"
	Version = uint32(2)
)

type FileInfo struct {
	Path  string
	Mtime int64
	Size  int64
	Hash  [16]byte
}

// Save writes files to path in the fm binary format.
func Save(path string, files []FileInfo) error {
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	w := func(v any) error { return binary.Write(f, binary.LittleEndian, v) }

	if _, err := f.WriteString(magic); err != nil {
		return err
	}
	if err := w(Version); err != nil {
		return err
	}
	if err := w(int64(time.Now().Unix())); err != nil {
		return err
	}
	if err := w(int32(len(files))); err != nil {
		return err
	}
	for _, fi := range files {
		pb := []byte(fi.Path)
		if err := w(int32(len(pb))); err != nil {
			return err
		}
		if _, err := f.Write(pb); err != nil {
			return err
		}
		if err := w(fi.Mtime); err != nil {
			return err
		}
		if err := w(fi.Size); err != nil {
			return err
		}
		if _, err := f.Write(fi.Hash[:]); err != nil {
			return err
		}
	}
	return nil
}

// Load reads a baseline file and returns its entries and creation time.
// Returns a descriptive error if the file is missing, corrupt, or the wrong version.
func Load(path string) ([]FileInfo, time.Time, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, time.Time{}, err
	}
	defer f.Close()

	buf := make([]byte, 4)
	if _, err := io.ReadFull(f, buf); err != nil || string(buf) != magic {
		return nil, time.Time{}, fmt.Errorf("baseline file %q has invalid format (bad magic)", path)
	}

	var ver uint32
	if err := binary.Read(f, binary.LittleEndian, &ver); err != nil {
		return nil, time.Time{}, fmt.Errorf("baseline file %q is truncated", path)
	}
	if ver != Version {
		return nil, time.Time{}, fmt.Errorf("baseline file %q has unsupported version %d (expected %d)", path, ver, Version)
	}

	var ts int64
	var count int32
	if err := binary.Read(f, binary.LittleEndian, &ts); err != nil {
		return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted (truncated header)", path)
	}
	if err := binary.Read(f, binary.LittleEndian, &count); err != nil {
		return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted (truncated header)", path)
	}

	entries := make([]FileInfo, 0, count)
	for i := int32(0); i < count; i++ {
		var pathLen int32
		if err := binary.Read(f, binary.LittleEndian, &pathLen); err != nil || pathLen <= 0 || pathLen > 4096 {
			return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted", path)
		}
		pb := make([]byte, pathLen)
		if _, err := io.ReadFull(f, pb); err != nil {
			return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted", path)
		}
		var fi FileInfo
		fi.Path = string(pb)
		if err := binary.Read(f, binary.LittleEndian, &fi.Mtime); err != nil {
			return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted", path)
		}
		if err := binary.Read(f, binary.LittleEndian, &fi.Size); err != nil {
			return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted", path)
		}
		if _, err := io.ReadFull(f, fi.Hash[:]); err != nil {
			return nil, time.Time{}, fmt.Errorf("baseline file %q is corrupted", path)
		}
		entries = append(entries, fi)
	}

	return entries, time.Unix(ts, 0), nil
}
