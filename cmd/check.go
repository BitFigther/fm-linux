/*
 * Copyright (c) 2025 BitFigther
 * Licensed under the MIT License
 */

package cmd

import (
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/spf13/cobra"

	"fm/internal/baseline"
	"fm/internal/scanner"
)

var checkCmd = &cobra.Command{
	Use:   "check <directory>...",
	Short: "Check for changes against the baseline",
	Args:  cobra.MinimumNArgs(1),
	RunE:  runCheck,
}

func runCheck(cmd *cobra.Command, dirs []string) error {
	fmt.Printf("Checking for changes in: %s\n", strings.Join(dirs, " "))

	bl, createdAt, loadedFrom, err := loadFirstBaseline(resolvedBaselineFiles())
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %s\n", err)
		return ErrSilent
	}
	if loadedFrom == "" {
		fmt.Fprintln(os.Stderr, "Error: Baseline file not found.")
		fmt.Fprintln(os.Stderr, "Please create a baseline first using the baseline command.")
		return ErrSilent
	}
	fmt.Printf("Baseline loaded: %d files (Created: %s)\n", len(bl), createdAt.Format("2006-01-02 15:04:05"))
	fmt.Println("Processing...")

	result, err := scanner.Check(dirs, bl, scanner.Options{ExcludePatterns: excludePatterns()})
	if err != nil {
		return ErrSilent
	}

	for _, m := range result.Modified {
		fmt.Printf("%sChange detected: %s%s\n", cYellow(), m.Path, cReset())
		if m.MtimeChanged {
			fmt.Printf("  Modified time: %s -> %s\n",
				time.Unix(m.OldMtime, 0).Format("20060102_150405"),
				time.Unix(m.NewMtime, 0).Format("20060102_150405"))
		}
		if m.SizeChanged {
			fmt.Printf("  Size: %d -> %d\n", m.OldSize, m.NewSize)
		}
		if m.HashChanged {
			fmt.Printf("  MD5 hash: %x -> %x\n", m.OldHash, m.NewHash)
		}
	}
	for _, a := range result.Added {
		fmt.Printf("%sNew file: %s (MD5: %x)%s\n", cGreen(), a.Path, a.Hash, cReset())
	}
	for _, d := range result.Deleted {
		fmt.Printf("%sDeleted file: %s%s\n", cRed(), d, cReset())
	}

	total := result.TotalChanges()
	fmt.Println("\n=== Result ===")
	if result.Unverified > 0 {
		fmt.Fprintf(os.Stderr, "Warning: %d file(s) could not be verified (read error or hash failure).\n", result.Unverified)
	}
	switch {
	case total > 0:
		fmt.Printf("Changes detected: %d file(s) changed\n", total)
		return NewExitError(2)
	case result.Unverified > 0:
		fmt.Printf("No changes confirmed, but %d file(s) could not be verified.\n", result.Unverified)
		return NewExitError(1)
	default:
		fmt.Println("No changes: No files were changed")
	}
	return nil
}

// loadFirstBaseline tries each path in order and loads the first one that
// exists. A missing file is not an error; any other failure (bad format,
// truncation, ...) is.
func loadFirstBaseline(paths []string) ([]baseline.FileInfo, time.Time, string, error) {
	for _, p := range paths {
		entries, ts, err := baseline.Load(p)
		if err != nil {
			if os.IsNotExist(err) {
				continue
			}
			return nil, time.Time{}, "", err
		}
		return entries, ts, p, nil
	}
	return nil, time.Time{}, "", nil
}
