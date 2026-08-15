/*
 * Copyright (c) 2025 BitFigther
 * Licensed under the MIT License
 */

package cmd

import (
	"fmt"
	"os"
	"strings"

	"github.com/spf13/cobra"

	"fm/internal/baseline"
	"fm/internal/scanner"
)

var baselineCmd = &cobra.Command{
	Use:   "baseline <directory>...",
	Short: "Create a baseline snapshot of the specified directories",
	Args:  cobra.MinimumNArgs(1),
	RunE:  runBaseline,
}

func runBaseline(cmd *cobra.Command, dirs []string) error {
	fmt.Printf("Creating baseline for: %s\n", strings.Join(dirs, " "))
	fmt.Println("Processing...")

	files, unverified, err := scanner.Baseline(dirs, scanner.Options{ExcludePatterns: excludePatterns()})
	if err != nil {
		return ErrSilent
	}
	if unverified > 0 {
		fmt.Fprintf(os.Stderr, "Warning: %d file(s) could not be read and were excluded from the baseline.\n", unverified)
	}

	var failed bool
	for _, p := range resolvedBaselineFiles() {
		if err := baseline.Save(p, files); err != nil {
			fmt.Fprintf(os.Stderr, "Error: Failed to write baseline file: %s\n", p)
			failed = true
			continue
		}
		fmt.Printf("Create baseline file : %s \n", p)
		fmt.Printf("Baseline saved: %d files\n", len(files))
	}
	if failed {
		return ErrSilent
	}
	return nil
}
