# Development Guide

## Running the Kernel

```bash
make clean && make run
```

**Behavior:** The kernel boots, initializes, then enters an idle loop (`wfe`). It will never exit on its own. Terminate manually with Ctrl-A X.

### Running the Kernel (Claude Code)

Use the Bash tool's `timeout` parameter. The command will be moved to background when timeout expires:

```
Bash tool with timeout: 5000
command: make clean && make run
```

Then check output with `cat <output_file>` and terminate with `KillShell`. Exit code 137 is expected (SIGKILL = 128 + 9).

## Running Tests

```bash
make clean && make run-test
```

**Behavior:** Tests normally complete and exit via semihosting. However, tests can hang on errors (e.g., exceptions, infinite loops). Use a timeout to handle hangs.

### Running Tests (Claude Code)

Use the Bash tool's `timeout` parameter (5 seconds is sufficient):

```
Bash tool with timeout: 5000
command: make clean && make run-test
```

If tests pass, the command completes normally. If tests hang, the command moves to background and can be killed.

## Code Quality

**No compiler warnings:** All code must compile without warnings.
