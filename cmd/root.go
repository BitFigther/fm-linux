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
)

const (
	defaultBaseline  = "/tmp/fm_baseline.dat"
	maxBaselineFiles = 8
)

var (
	baselineFilePaths []string
	excludeRaw        []string
	noColor           bool
)

var rootCmd = &cobra.Command{
	Use:           "fm",
	Short:         "File monitor — detect file modifications using MD5 hash",
	SilenceUsage:  true,
	SilenceErrors: true,
	RunE: func(cmd *cobra.Command, args []string) error {
		if len(args) == 0 {
			return cmd.Help()
		}
		return fmt.Errorf("unknown command %q for %q", args[0], cmd.CommandPath())
	},
}

// Execute runs the fm CLI and returns any error from the executed command.
func Execute() error {
	return rootCmd.Execute()
}

func init() {
	rootCmd.PersistentFlags().StringSliceVarP(&baselineFilePaths, "baseline-file", "b",
		nil, "Baseline file path(s), comma-separated (default: "+defaultBaseline+")")
	rootCmd.PersistentFlags().StringArrayVarP(&excludeRaw, "exclude", "e",
		nil, "Exclude glob pattern(s); may be comma-separated or repeated")
	rootCmd.PersistentFlags().BoolVar(&noColor, "no-color", false, "Disable colored output")

	rootCmd.AddCommand(baselineCmd, checkCmd, resetCmd)
}

// excludePatterns expands any comma-separated --exclude values into a flat
// pattern list, so "-e a,b" and "-e a -e b" behave identically.
func excludePatterns() []string {
	var out []string
	for _, raw := range excludeRaw {
		for _, p := range strings.Split(raw, ",") {
			if p = strings.TrimSpace(p); p != "" {
				out = append(out, p)
			}
		}
	}
	return out
}

// resolvedBaselineFiles returns the configured baseline paths, capped at
// maxBaselineFiles.
func resolvedBaselineFiles() []string {
	paths := baselineFilePaths
	if len(paths) == 0 {
		paths = []string{defaultBaseline}
	}
	if len(paths) > maxBaselineFiles {
		fmt.Fprintf(os.Stderr, "Warning: Too many baseline files. Only the first %d will be used.\n", maxBaselineFiles)
		paths = paths[:maxBaselineFiles]
	}
	return paths
}
