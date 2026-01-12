# Development Guide

## Running Tests

```bash
make clean && make run-test
```

**Note:** Tests run in QEMU and may hang since this is OS development. If tests hang, you can run them in the background (e.g., using `&` in the shell).

## Code Quality

**No compiler warnings:** All code must compile without warnings.
