# Incremental Builds

Goal: `make run` and `make test` skip recompilation when source hasn't changed.
Must work on both macOS (host) and slopix (self-hosted).

## Current State

### Makefile

`run` and `test` depend on `clean`:

```makefile
run: clean disk.img
test: clean disk-test.img
```

`clean` deletes everything — `.bin/`, `.build/`, and disk images. Every `make run`
or `make test` is a full rebuild from scratch.

The `build` and `build-test` targets are `.PHONY`, so Make always re-runs them
even if nothing changed. There are no file-level dependencies for the cross-built
outputs.

Host tool targets have incomplete dependencies:

```makefile
.bin/cc: .bin/build | .bin
```

`.bin/cc` depends on `.bin/build` but not on the actual cc source files
(`cmd/cc/*.c`, `cmd/cc/*.h`). Changing cc source won't trigger a rebuild.

### Build tool (`lib/build.h`)

`compile()` and `assemble()` unconditionally run the compiler and assembler for
every source file. No mtime checks. No caching.

`build_subdir()` (line 449) calls `remove_recursive(".build")` after moving
outputs to the parent `.build/out`. This destroys all intermediate object files
from each subdirectory. Even without `make clean`, object files are lost after
every build.

### Filesystem — no mtime

The on-disk inode (`struct dinode` in `kernel/fs.h:50`) has no mtime field:

```c
struct dinode {
    unsigned short type;
    unsigned short major;
    unsigned short minor;
    unsigned short nlink;
    unsigned int size;
    unsigned int addrs[NDIRECT + 2];
};
```

The in-memory inode (`struct inode` in `kernel/fs.h:66`) mirrors this — no mtime.

`fs_writei()` updates size but not any timestamp. `fs_ialloc()` zeroes the inode
but doesn't stamp creation time. `fs_stati()` (`kernel/fs.c:521`) populates
dev, ino, mode, nlink, size — never mtime.

The kernel's `struct stat` (`kernel/fs.h:81`) doesn't even have an `st_mtime`
field. The libc `struct stat` (`libc/include/sys/stat.h:9`) does have `st_mtime`
but it is never populated by the kernel.

`mkfs` also writes no timestamps — `ialloc()` and `iappend()` only set type,
nlink, size, and addrs.

### Time source exists

The PL031 RTC is initialized (`kernel/rtc.c`) and `rtc_read()` returns Unix
epoch seconds. There's a `sys_time()` syscall and libc `time()` wrapper. So the
kernel can get current wall-clock time — the infrastructure exists, it's just not
wired into the filesystem.

## Gaps

### 1. Makefile forces full rebuild

`run: clean disk.img` means every run starts from zero. Removing the `clean`
dependency isn't enough — `build` is `.PHONY` and always re-runs.

`disk.img` is a real file. All sources are real files. Make can compare their
mtimes directly — no stamp files needed. The disk image target should depend on
the actual source files so Make's own mtime logic decides whether to rebuild.

### 2. Build tool has no mtime comparison

`compile()` and `assemble()` need a `needs_rebuild(src, obj)` check:
compare source mtime against output mtime, skip if output is newer.

On macOS this works today — host `stat()` populates `st_mtime`. On slopix this
is blocked by gap #3.

### 3. Filesystem lacks mtime (slopix blocker)

Adding mtime requires changes across the full stack:

- `struct dinode` — add `unsigned int mtime` field (changes on-disk format)
- `struct inode` — add `unsigned int mtime` field
- `fs_ilock()` — copy mtime from dinode to inode
- `fs_iupdate()` — copy mtime from inode to dinode
- `fs_ialloc()` — set `mtime = rtc_read()` on creation
- `fs_writei()` — set `ip->mtime = rtc_read()` on write
- `fs_stati()` — set `st->st_mtime = ip->mtime`
- Kernel `struct stat` — add `time_t st_mtime` (or unify with libc's)
- `mkfs` — update its copy of `struct dinode` to match, optionally stamp
  host file mtimes into the image

This is a filesystem format change. Existing disk images become incompatible.

### 4. build_subdir destroys intermediates

`build_subdir()` moves `.build/out/*` to the parent then `remove_recursive`s
the entire `.build/`. This kills `.build/obj/*.o` which would be needed for
incremental comparison on the next run.

Options:
- Keep `.build/obj/` in each subdirectory (don't clean it)
- Keep object files alongside outputs in `.build/out/`
- Use a single top-level `.build/` instead of per-subdirectory ones

### 5. Header dependency tracking

Mtime comparison of `foo.c` vs `foo.o` catches source changes but misses header
changes. If `fs.h` changes, all files that include it need recompilation.

Options:
- Conservative: if any `.h` file changed, rebuild all `.c` files
- Precise: use `cc -MMD` to emit `.d` dependency files, parse them to check
  all transitive includes
- The slopix `cc` would need to support `-MMD` or equivalent

## Approach

### Phase 1: Makefile (host-side, no kernel changes)

Remove `clean` from `run` and `test` dependencies. Make `disk.img` depend on
actual source files so Make's mtime comparison decides whether to rebuild.
No stamp files — `disk.img` is the real output and all sources are real inputs.

```makefile
SOURCES := $(shell find kernel libc boot cmd lib -name '*.c' -o -name '*.h' -o -name '*.S')
TESTDATA := testdata/hello.txt testdata/large.txt
TOOLS := .bin/cc .bin/as .bin/ld .bin/ar .bin/mkfs

disk.img: $(SOURCES) $(TOOLS)
	$(CROSS_BUILD_ENV) $(BUILD)
	$(MKFS) $@ -s 102400 -i 1024 ...

disk-test.img: $(SOURCES) $(TESTDATA) $(TOOLS)
	$(CROSS_BUILD_ENV) RUN_TESTS=1 $(BUILD)
	$(MKFS) $@ -s 8192 ...

run: disk.img
	$(QEMU_DISK)

test: disk-test.img
	$(QEMU_TEST) 2>&1 | ./scripts/format-tests.sh
```

`disk.img` and `disk-test.img` are independent real files with independent
mtimes. Make tracks them separately — `make run` then `make test` with no edits
in between skips both builds. The two builds share `.build/out/` but produce
different kernel binaries (`kernel.bin` vs `kernel-test.bin`), so they don't
conflict.

`disk-test.img` additionally depends on `$(TESTDATA)` since those files are
baked into the test disk image by mkfs.

Fix host tool dependencies to include actual source files. Memory layouts are
hardcoded in `cmd/ld`, so changing `cmd/ld/*.c` rebuilds `.bin/ld`, which is
already in the `disk.img` dependency list — no linker scripts to track.

```makefile
CC_SOURCES := $(shell find cmd/cc -name '*.c' -o -name '*.h')
.bin/cc: $(CC_SOURCES) lib/build.h .bin/build | .bin
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.bin cmd/cc
```

Same pattern for `.bin/as`, `.bin/ld`, `.bin/ar`, `.bin/mkfs`.

`$(shell find ...)` runs on every Make invocation but is milliseconds for this
project size. It auto-discovers new files — add `kernel/foo.c` and it becomes
a dependency immediately.

This works before the build tool is incremental. Even if `$(BUILD)` recompiles
everything, Make gates on "did anything change at all?" — so `make run` after
`make run` with no edits is instant. Phase 2 optimizes the case where you
changed one file and want to skip recompiling the other 40.

### Phase 2: Build tool mtime checks

Add `needs_rebuild()` to `lib/build.h`:

```c
int needs_rebuild(const char *src, const char *out) {
    struct stat src_st, out_st;
    if (stat(out, &out_st) != 0) return 1;  // output missing
    if (stat(src, &src_st) != 0) return 1;  // source missing (error)
    return src_st.st_mtime > out_st.st_mtime;
}
```

Guard `compile()` and `assemble()` with this check. Also guard `link_objs()`
and `archive_objs()` — skip link if all objects are older than the output.

Stop `build_subdir()` from deleting `.build/obj/` so object files persist.

This works on macOS immediately. On slopix it's a no-op until phase 3.

#### Test mode vs normal mode: stale object files

The kernel compiles with `-DRUN_TESTS` in test mode. This is a compile-time
`#ifdef` — test macros either expand to real test functions or to `((void)0)`
no-ops. The resulting .o files are different even though the source is identical.

If `needs_rebuild()` only checks source mtime vs object mtime, switching between
`make test` and `make run` reuses stale .o files from the other mode:

1. `make test` — kernel .o files compiled with `-DRUN_TESTS`
2. `make run` — source unchanged, `needs_rebuild()` says skip — links with
   test-enabled .o files — kernel runs tests when it shouldn't

Only the kernel is affected. libc, boot, and cmd compile identically in both
modes.

Options:

**A. Separate object directories** — use `.build/obj/` for normal builds and
`.build/obj-test/` for test builds. Both caches coexist, no cross-contamination.
Simple for two modes. Doesn't scale to arbitrary flag combinations (DEBUG=1 would
need obj-debug/, obj-test-debug/, etc).

**B. Flag file** — store current flags in `.build/obj/.flags`. On build start,
compare against stored flags. If different, wipe obj dir and rebuild. Handles
arbitrary flags. Downside: switching modes invalidates the cache. In practice
this is rare — you usually do repeated `make test` or repeated `make run`, not
alternating.

**C. Runtime test detection** — move test mode from compile-time `#ifdef` to a
runtime decision. Test code is always compiled in. At boot, check a kernel
cmdline parameter (e.g. `runtests`) to decide whether to run tests. The kernel
already has cmdline support and QEMU can pass boot args.

This eliminates the two-mode build problem entirely: one kernel binary, same .o
files always. `make test` after `make run` never recompiles — only rebuilds the
disk image (different test data/binaries).

Tradeoff: kernel binary is larger (test code always included) and test code is
present in the "production" kernel. For this project's size the binary difference
is negligible.

### Phase 3: Filesystem mtime (slopix support)

Add mtime to `struct dinode` and `struct inode`. Wire `rtc_read()` into
`fs_writei()`, `fs_ialloc()`, and `fs_stati()`. Update `mkfs` to match.

After this, `needs_rebuild()` works inside slopix too.

### Phase 4: Header dependencies (optional)

Add `-MMD` support to the slopix `cc` preprocessor to emit `.d` files.
Parse `.d` files in `needs_rebuild()` to check all transitive includes.

Until then, a conservative fallback (rebuild all if any header changed) is
practical for the project's size.

## Open Questions

- Should `build_subdir()` keep per-subdirectory `.build/` dirs, or should we
  move to a single top-level `.build/` tree? Per-subdirectory is simpler to
  retrofit but wastes space with duplicate libc objects.
- Should `mkfs` preserve host file mtimes when creating disk images? This would
  let incremental builds inside slopix detect "nothing changed since the image
  was created" without rebuilding everything on first boot.
- Is the dinode format change acceptable? It invalidates existing disk images.
  Could version the superblock to detect old format.
