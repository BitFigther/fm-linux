/*
 * Copyright (c) 2025 BitFigther
 * Licensed under the MIT License
 */

package main

import (
	"errors"
	"fmt"
	"os"

	"fm/cmd"
)

func main() {
	if err := cmd.Execute(); err != nil {
		var exitErr *cmd.ExitError
		switch {
		case errors.As(err, &exitErr):
			os.Exit(exitErr.Code)
		case errors.Is(err, cmd.ErrSilent):
			os.Exit(1)
		default:
			fmt.Fprintln(os.Stderr, "Error:", err)
			os.Exit(1)
		}
	}
}
