# Development Guide

## Running the Kernel

```bash
make clean && make run
```

**Note:** The kernel runs in QEMU and will not exit on its own. Run in the background with `&` or terminate with Ctrl-A X.

## Running Tests

```bash
make clean && make run-test
```

**Note:** Tests run in QEMU and may hang since this is OS development. If tests hang, you can run them in the background (e.g., using `&` in the shell).

## Code Quality

**No compiler warnings:** All code must compile without warnings.
