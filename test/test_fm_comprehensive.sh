#!/bin/bash

# fm.c Comprehensive Test Script
# Copyright (c) 2025 BitFigther

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Build directory
BUILD_DIR="../build"
FM_BINARY="$BUILD_DIR/fm"

# Test directories
TEST_BASE_DIR="/tmp/fm_test_$$"
TEST_DIR1="$TEST_BASE_DIR/dir1"
TEST_DIR2="$TEST_BASE_DIR/dir2"
BASELINE_FILE="$TEST_BASE_DIR/baseline.dat"

# Function to print test results
print_test_result() {
    local test_name="$1"
    local result="$2"
    local message="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if [ "$result" = "PASS" ]; then
        echo -e "${GREEN}[PASS]${NC} $test_name: $message"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}[FAIL]${NC} $test_name: $message"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
}

# Function to setup test environment
setup_test_env() {
    echo -e "${YELLOW}Setting up test environment...${NC}"
    
    # Clean up any existing test directories
    rm -rf "$TEST_BASE_DIR"
    
    # Create test directories
    mkdir -p "$TEST_DIR1" "$TEST_DIR2"
    
    # Create test files
    echo "test file 1" > "$TEST_DIR1/file1.txt"
    echo "test file 2" > "$TEST_DIR1/file2.txt"
    mkdir -p "$TEST_DIR1/subdir"
    echo "subdir file" > "$TEST_DIR1/subdir/file3.txt"
    echo "binary data" > "$TEST_DIR2/binary.bin"
    
    # Check if fm binary exists
    if [ ! -f "$FM_BINARY" ]; then
        echo -e "${RED}Error: fm binary not found at $FM_BINARY${NC}"
        echo "Please build the project first: make"
        
        # For CI environments, try to find the binary
        if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
            echo "CI environment detected, searching for fm binary..."
            if [ -f "./fm" ]; then
                FM_BINARY="./fm"
                echo "Found fm binary at: $FM_BINARY"
            elif [ -f "../fm" ]; then
                FM_BINARY="../fm"
                echo "Found fm binary at: $FM_BINARY"
            else
                echo "fm binary not found in CI environment"
                exit 1
            fi
        else
            exit 1
        fi
    fi
    
    echo "Using fm binary: $FM_BINARY"
}

# Function to cleanup test environment
cleanup_test_env() {
    echo -e "${YELLOW}Cleaning up test environment...${NC}"
    
    # For CI environments, preserve test data for debugging
    if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
        if [ $FAILED_TESTS -gt 0 ]; then
            echo "Preserving test data for CI debugging..."
            echo "Test directory: $TEST_BASE_DIR"
            return
        fi
    fi
    
    rm -rf "$TEST_BASE_DIR"
}

# Test functions

# TC001: Empty directory baseline creation
test_empty_dir_baseline() {
    local empty_dir="$TEST_BASE_DIR/empty"
    mkdir -p "$empty_dir"
    
    if $FM_BINARY --baseline "$empty_dir" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1; then
        if [ -f "$BASELINE_FILE" ]; then
            print_test_result "TC001" "PASS" "Empty directory baseline created"
        else
            print_test_result "TC001" "FAIL" "Baseline file not created"
        fi
    else
        print_test_result "TC001" "FAIL" "Baseline creation failed"
    fi
    rm -f "$BASELINE_FILE"
}

# TC002: Single file baseline creation
test_single_file_baseline() {
    local single_dir="$TEST_BASE_DIR/single"
    mkdir -p "$single_dir"
    echo "single file content" > "$single_dir/single.txt"
    
    if $FM_BINARY --baseline "$single_dir" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1; then
        if [ -f "$BASELINE_FILE" ]; then
            print_test_result "TC002" "PASS" "Single file baseline created"
        else
            print_test_result "TC002" "FAIL" "Baseline file not created"
        fi
    else
        print_test_result "TC002" "FAIL" "Single file baseline creation failed"
    fi
    rm -f "$BASELINE_FILE"
}

# TC003: Multiple files with subdirectories baseline
test_multiple_files_baseline() {
    if $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1; then
        if [ -f "$BASELINE_FILE" ]; then
            print_test_result "TC003" "PASS" "Multiple files baseline created"
        else
            print_test_result "TC003" "FAIL" "Baseline file not created"
        fi
    else
        print_test_result "TC003" "FAIL" "Multiple files baseline creation failed"
    fi
}

# TC006: No changes detection
test_no_changes() {
    # Create baseline first
    $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Check immediately without changes
    local output=$($FM_BINARY --check "$TEST_DIR1" --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "No changes"; then
        print_test_result "TC006" "PASS" "No changes correctly detected"
    else
        print_test_result "TC006" "FAIL" "No changes detection failed (exit: $exit_code)"
    fi
    rm -f "$BASELINE_FILE"
}

# TC007: New file detection
test_new_file_detection() {
    # Create baseline
    $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Add new file
    echo "new file content" > "$TEST_DIR1/new_file.txt"
    
    local output=$($FM_BINARY --check "$TEST_DIR1" --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 2 ] && echo "$output" | grep -q "New file.*new_file.txt"; then
        print_test_result "TC007" "PASS" "New file correctly detected"
    else
        print_test_result "TC007" "FAIL" "New file detection failed (exit: $exit_code)"
    fi
    
    rm -f "$TEST_DIR1/new_file.txt" "$BASELINE_FILE"
}

# TC008: Deleted file detection
test_deleted_file_detection() {
    # Create baseline
    $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Delete a file
    rm -f "$TEST_DIR1/file2.txt"
    
    local output=$($FM_BINARY --check "$TEST_DIR1" --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 2 ] && echo "$output" | grep -q "Deleted file.*file2.txt"; then
        print_test_result "TC008" "PASS" "Deleted file correctly detected"
    else
        print_test_result "TC008" "FAIL" "Deleted file detection failed (exit: $exit_code)"
    fi
    
    # Restore file for other tests
    echo "test file 2" > "$TEST_DIR1/file2.txt"
    rm -f "$BASELINE_FILE"
}

# TC009: File content change detection
test_content_change_detection() {
    # Create baseline
    $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Modify file content
    echo "modified content" > "$TEST_DIR1/file1.txt"
    
    local output=$($FM_BINARY --check "$TEST_DIR1" --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 2 ] && echo "$output" | grep -q "Change detected.*file1.txt"; then
        print_test_result "TC009" "PASS" "Content change correctly detected"
    else
        print_test_result "TC009" "FAIL" "Content change detection failed (exit: $exit_code)"
    fi
    
    # Restore file
    echo "test file 1" > "$TEST_DIR1/file1.txt"
    rm -f "$BASELINE_FILE"
}

# TC013: Reset baseline file
test_reset_baseline() {
    # Create baseline first
    $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Reset baseline
    local output=$($FM_BINARY --reset --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] && [ ! -f "$BASELINE_FILE" ] && echo "$output" | grep -q "Baseline file deleted"; then
        print_test_result "TC013" "PASS" "Baseline reset successful"
    else
        print_test_result "TC013" "FAIL" "Baseline reset failed (exit: $exit_code)"
    fi
}

# TC014: Reset non-existent baseline
test_reset_nonexistent_baseline() {
    local output=$($FM_BINARY --reset --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "Baseline file not found"; then
        print_test_result "TC014" "PASS" "Reset non-existent baseline handled correctly"
    else
        print_test_result "TC014" "FAIL" "Reset non-existent baseline failed (exit: $exit_code)"
    fi
}

# TC016: Exclude pattern test
test_exclude_pattern() {
    # Create test file to exclude
    echo "exclude me" > "$TEST_DIR1/exclude.tmp"
    
    # Create baseline with exclude pattern
    $FM_BINARY --baseline "$TEST_DIR1" --exclude "*.tmp" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Check that excluded file is not in baseline
    local output=$($FM_BINARY --check "$TEST_DIR1" --exclude "*.tmp" --baseline-file "$BASELINE_FILE" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "No changes"; then
        print_test_result "TC016" "PASS" "Exclude pattern working correctly"
    else
        print_test_result "TC016" "FAIL" "Exclude pattern failed (exit: $exit_code)"
    fi
    
    rm -f "$TEST_DIR1/exclude.tmp" "$BASELINE_FILE"
}

# TC022: Color output test
test_color_output() {
    # Create baseline
    $FM_BINARY --baseline "$TEST_DIR1" --baseline-file "$BASELINE_FILE" >/dev/null 2>&1
    
    # Add new file and check with color
    echo "new file" > "$TEST_DIR1/color_test.txt"
    local output_color=$($FM_BINARY --check "$TEST_DIR1" --baseline-file "$BASELINE_FILE" 2>&1)
    
    # Check without color
    local output_no_color=$($FM_BINARY --check "$TEST_DIR1" --baseline-file "$BASELINE_FILE" --no-color 2>&1)
    
    if echo "$output_color" | grep -q $'\033' && ! echo "$output_no_color" | grep -q $'\033'; then
        print_test_result "TC022" "PASS" "Color output control working"
    else
        print_test_result "TC022" "FAIL" "Color output control failed"
    fi
    
    rm -f "$TEST_DIR1/color_test.txt" "$BASELINE_FILE"
}

# TC024: No arguments test
test_no_arguments() {
    local output=$($FM_BINARY 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "Usage:"; then
        print_test_result "TC024" "PASS" "No arguments handled correctly"
    else
        print_test_result "TC024" "FAIL" "No arguments handling failed (exit: $exit_code)"
    fi
}

# TC027: Non-existent directory test
test_nonexistent_directory() {
    local output=$($FM_BINARY --baseline "/non/existent/dir" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -eq 1 ]; then
        print_test_result "TC027" "PASS" "Non-existent directory handled correctly"
    else
        print_test_result "TC027" "FAIL" "Non-existent directory handling failed (exit: $exit_code)"
    fi
}

# Main test execution
main() {
    echo -e "${YELLOW}=== FM.C Comprehensive Test Suite ===${NC}"
    echo "Starting tests..."
    
    # Detect environment
    if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
        echo "Running in CI environment"
    fi
    
    echo
    
    setup_test_env
    
    # Run basic functionality tests
    echo -e "${YELLOW}Running Basic Functionality Tests...${NC}"
    test_empty_dir_baseline
    test_single_file_baseline
    test_multiple_files_baseline
    test_no_changes
    test_new_file_detection
    test_deleted_file_detection
    test_content_change_detection
    test_reset_baseline
    test_reset_nonexistent_baseline
    
    # Run option tests
    echo -e "${YELLOW}Running Option Tests...${NC}"
    test_exclude_pattern
    test_color_output
    
    # Run error handling tests
    echo -e "${YELLOW}Running Error Handling Tests...${NC}"
    test_no_arguments
    test_nonexistent_directory
    
    cleanup_test_env
    
    # Print summary
    echo
    echo -e "${YELLOW}=== Test Summary ===${NC}"
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    
    # Output summary for CI
    if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
        echo "::notice title=Test Summary::Total: $TOTAL_TESTS, Passed: $PASSED_TESTS, Failed: $FAILED_TESTS"
        
        if [ $FAILED_TESTS -gt 0 ]; then
            echo "::error title=Test Failures::$FAILED_TESTS out of $TOTAL_TESTS tests failed"
        fi
    fi
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed.${NC}"
        exit 1
    fi
}

# Run main function
main "$@"