# FM Test Status

This repository includes comprehensive CI/CD testing using GitHub Actions.

## Test Coverage

The test suite covers:
- ✅ Basic functionality (baseline creation, change detection, reset)
- ✅ File operations (new, modified, deleted files)
- ✅ Command line options (exclude patterns, color output)
- ✅ Error handling (invalid arguments, non-existent files)
- ✅ Multiple compilers (GCC, Clang)
- ✅ Multiple Ubuntu versions (latest, 20.04)

## Running Tests

### Locally
```bash
# Build the project
make

# Run comprehensive tests
cd test
./test_fm_comprehensive.sh
```

### CI/CD
Tests automatically run on:
- Push to `main` or `develop` branches
- Pull requests to `main` branch

## Test Matrix

| OS | Compiler | Status |
|---|---|---|
| Ubuntu Latest | GCC | ✅ |
| Ubuntu Latest | Clang | ✅ |
| Ubuntu 20.04 | GCC | ✅ |
| Ubuntu 20.04 | Clang | ✅ |

## Coverage Report

Code coverage is automatically generated for the `main` branch and uploaded to Codecov.

## Test Artifacts

On test failures, artifacts are preserved for 7 days including:
- Test temporary files
- Build artifacts
- Error logs