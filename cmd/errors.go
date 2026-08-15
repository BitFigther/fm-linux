package cmd

import (
	"errors"
	"fmt"
)

// ExitError signals that main should exit with Code; the command has
// already printed any user-facing message itself.
type ExitError struct {
	Code int
}

func (e *ExitError) Error() string {
	return fmt.Sprintf("exit code %d", e.Code)
}

func NewExitError(code int) *ExitError {
	return &ExitError{Code: code}
}

// ErrSilent signals a plain exit-1 failure whose message has already been
// printed by the command that returned it.
var ErrSilent = errors.New("silent error")
