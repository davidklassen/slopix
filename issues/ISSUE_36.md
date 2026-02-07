# ISSUE_36: Exec command line limit and compiler codegen bug with large stack frames

## Severity
Medium

## Location
- `libc/stdlib.c` — `execvp()`
- `kernel/syscall.c` — `sys_exec()`
- `.tools/cc` — codegen for large stack frames

## Description

### Command line buffer limit

The `execvp()` libc function concatenates all argv entries into a single command line string passed to the `exec` syscall. Both the libc buffer and the kernel's `sys_exec` buffer were 1024 bytes. When self-hosting, the kernel linker command with 54 object files produces ~1700 bytes, causing silent truncation mid-path.

The buffers were bumped to 2048 bytes as an immediate fix. This is sufficient for the current kernel link command but will break again as more source files are added.

### Proper fix options

1. **Larger buffers** — simple but keeps hitting limits as the project grows.
2. **Response file support** — the linker reads arguments from `@file` instead of the command line. Requires changes to both `cmd/ld` and `lib/build.h`.
3. **Argv-based exec syscall** — change `sys_exec` to accept `(path, argv)` instead of a single command line string, matching the POSIX interface. Eliminates the concatenation/parsing roundtrip entirely.

### Compiler codegen bug with large stack frames

While investigating, a separate bug was found in the custom compiler (`.tools/cc`). When a function's stack frame exceeds approximately 3400 bytes, the generated code produces incorrect memory accesses — writing to small invalid addresses (e.g., FAR=0x760, 0xa00) instead of the intended stack locations. The crash is a DATA ABORT in the kernel.

Observed behavior:
- `sys_exec` with `kcmd[1024]` (frame=2400) — works
- `sys_exec` with `kcmd[2000]` (frame=3376) — works
- `sys_exec` with `kcmd[2048]` (frame=3424) — crashes (with KSTACK_PAGES=8)
- `sys_exec` with `kcmd[4096]` (frame=5472) — crashes

The compiler correctly uses `mov x9, #N; sub sp, sp, x9` for large frame allocation, so the prologue itself is fine. The bug is likely in how variable addresses are computed when stack offsets exceed a certain threshold (possibly related to AArch64 addressing mode immediate limits in the codegen).

Making the buffer `static` avoids the issue by keeping it off the stack, but this masks the underlying compiler bug.
