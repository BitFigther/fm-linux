/*
 * Copyright (c) 2025 BitFigther
 * Licensed under the MIT License
 */

package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var resetCmd = &cobra.Command{
	Use:   "reset",
	Short: "Delete the baseline file",
	Args:  cobra.NoArgs,
	RunE:  runReset,
}

func runReset(cmd *cobra.Command, args []string) error {
	var failed bool
	for _, p := range resolvedBaselineFiles() {
		if err := os.Remove(p); err != nil {
			fmt.Fprintf(os.Stderr, "Baseline file not found: %s\n", p)
			failed = true
			continue
		}
		fmt.Printf("Baseline file deleted: %s\n", p)
	}
	if failed {
		return ErrSilent
	}
	return nil
}
