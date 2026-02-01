# Slopix Build System Redesign

This document captures research and design decisions for a new build system.

## Goals

1. **nob-style build scripts** - C programs as build system, no make/shell
2. **Clean working tree** - no build artifacts in source directories
3. **Easy install** - staging directory mirrors target filesystem
4. **Same build on host and slopix** - identical flow, different only in paths
5. **Everything is a slopix program** - no special-case "host-only" tools

## Research: nob.h

[nob.h](https://github.com/tsoding/nob.h) is a header-only C library for writing
build scripts in C. Core philosophy: "you should only need a C compiler to build
a C project."

### Key Features

```c
// Dynamic command arrays
Nob_Cmd cmd = {0};
nob_cmd_append(&cmd, "cc", "-o", "build/hello", "src/hello.c");
nob_cmd_run(&cmd);

// Self-rebuilding
NOB_GO_REBUILD_URSELF(argc, argv);

// Dependency checking (mtime-based)
if (nob_needs_rebuild("output", inputs, count)) {
    // rebuild
}

// File operations
nob_mkdir_if_not_exists("build/");
nob_copy_file("src", "dst");
```

### What We Take From nob.h

- **Pattern**: C programs as build scripts
- **Utilities**: Dynamic arrays, command execution, file operations

### What We Skip (For Now)

- **mtime-based rebuilds**: slopix stat() doesn't populate st_mtime yet
- **Self-rebuilding**: Requires mtime (see BACKLOG.md)
- **Parallel builds**: Sequential is fine for small OS
- **Platform abstraction**: We only target slopix (and POSIX hosts)

## Current State

### Source Duplication Problem

```
cmd/cc/      # Slopix compiler source (canonical)
tools/cc/    # Same source, tweaked for host, separate Makefile
```

Differences are minimal:

| File | Difference | Reason |
|------|------------|--------|
| main.c | Include path detection | tools/cc uses dirname(), cmd/cc hardcodes paths |
| main.c | Version string | cmd/cc has `--version` |
| codegen.c | Format string | cmd/cc avoids `%+ld` (libc compatibility) |
| tokenize.c | Stack allocation | cmd/cc uses malloc instead of 4KB VLA |

This is awkward:
- Two copies of nearly identical code
- Manual sync when either changes
- Build artifacts scattered in both places

**Resolution:** Keep `cmd/cc/` as canonical source. Make include paths configurable
via `-I` flags passed at compile time, not hardcoded. Remove `tools/cc/` entirely.

### Build Artifacts in Source Tree

```
cmd/cat/cat.c    # Source
cmd/cat/cat.s    # Artifact - pollutes working tree
cmd/cat/cat.o    # Artifact
cmd/cat.elf      # Binary at different level
```

### Missing Sources on Disk

Currently, only `cmd/cc/` sources are copied to the disk image. Sources for
`cmd/as/` and `cmd/ld/` are NOT included, preventing self-hosted rebuilds of
the assembler and linker. The new source sync will fix this.

## Proposed Design

### The `/bin/build` Tool

A generic build tool, similar to make. Behavior:

1. Look for `build.c` in current directory
2. If found: compile to `.build/build`, run it
3. If not found: fallback - compile single `.c` file directly (simple commands)

```
cd /src/cmd/cc && /bin/build     # finds build.c → compile & run
cd /src/cmd/cat && /bin/build    # no build.c → compiles cat.c directly
cd /src && /bin/build            # finds build.c → builds everything
```

Each directory is autonomous. `/bin/build` knows nothing about specific projects.

**Options:**
- `/bin/build clean` - removes `.build/` and `build/` in current directory
- `/bin/build --prefix=<path>` - output binaries to `<path>/` instead of `build/bin/`

The `--prefix` option is used for host bootstrap to output to `.bin/` instead of
the default `build/bin/`. The prefix is passed to the compiled build.c via the
`BUILD_PREFIX` environment variable.

**Fallback behavior** (no build.c):
- Find `*.c` file in current directory
- If zero or multiple `.c` files found: error (use build.c for complex cases)
- Compile: `$CC -I$INCLUDE_PATH -S -o .build/<name>.s <name>.c`
- Assemble: `$AS -o .build/<name>.o .build/<name>.s`
- Link: `$LD -o $BUILD_PREFIX/<dirname> .build/<name>.o $LIB_PATH/libc.a`
- Output to `$BUILD_PREFIX/<dirname>` (default: `build/bin/<dirname>`)

**Cache behavior**: The `.build/build` binary is always recompiled from build.c.
When slopix gains mtime support, this can be optimized to skip recompilation
if build.c and build.h are unchanged.

### Artifact Bubbling

Each build outputs to local `./build/` and `./.build/` directories. Parent builds
collect artifacts from children and propagate them upward:

```
cd /src/cmd/cc && /bin/build
→ creates ./build/bin/cc
→ creates ./.build/obj/*.o

cd /src/cmd && /bin/build
→ runs build in cc/
→ moves cc/build/* → ./build/*
→ cleans cc/build/, cc/.build/
→ repeats for as/, ld/, cat/, ...

cd /src && /bin/build
→ runs build in libc/
→ moves libc/build/* → ./build/*
→ runs build in cmd/
→ moves cmd/build/* → ./build/*
→ final ./build/ contains everything
```

Benefits:
- Each build works in isolation (no path calculations)
- Running sub-build directly works for quick testing
- Parent aggregates children's artifacts
- Clean tree after full build (intermediates removed)

### Directory Structure

```
slopix/
├── build.c                # Root: delegates to libc/, cmd/
├── lib/                   # Build utilities (create this directory)
│   └── build.h            # Shared build utilities (nob-style)
├── cmd/
│   ├── build.c            # Proxy: iterates subdirs
│   ├── build/             # The /bin/build tool itself
│   │   └── build.c
│   ├── cc/
│   │   ├── main.c, ...
│   │   └── build.c        # Knows cc sources
│   ├── as/
│   │   └── build.c
│   ├── cat/
│   │   └── cat.c          # Simple - no build.c, uses fallback
│   ├── mkfs/              # Disk image creator
│   ├── mkramfs/           # Initramfs creator
│   └── ...
├── libc/
│   ├── build.c            # Knows libc sources
│   ├── include/           # All headers (libc + compiler builtins)
│   └── ...
├── kernel/                # Stays separate - uses Makefile + GCC
│
├── Makefile               # Builds kernel (userspace via .bin/build)
├── .mkfsignore           # Exclusions for /src sync
│
├── .bin/                  # Host-native binaries (gitignored)
│   ├── build              # Build tool (runs on host)
│   ├── cc                 # Cross-compiler (host binary → aarch64 output)
│   ├── as                 # Cross-assembler
│   ├── ld                 # Cross-linker
│   ├── ar                 # Archive tool
│   ├── mkfs               # Disk image creator
│   └── mkramfs            # Initramfs creator
│
├── .build/                # Intermediate artifacts (gitignored)
│   └── obj/               # Object files
│
└── build/                 # Staging root - mirrors / (gitignored)
    ├── bin/
    ├── lib/
    │   └── libc.a
    └── src/               # Source tree copy (via mkfs -m)
```

**Note:** The `lib/` directory at project root is for build utilities only.
It must be created as part of Phase 1. This is separate from `libc/`.

### Include Path Consolidation

Currently there are two include directories:
- `cmd/cc/include/` - compiler builtins (stdarg.h, stddef.h, etc.)
- `libc/include/` - C library headers (stdio.h, stdlib.h, etc.)

Many headers are duplicated with minor differences. **Resolution:** Merge
`cmd/cc/include/` into `libc/include/` and delete `cmd/cc/include/`. This
gives a single `INCLUDE_PATH` for all builds.

### Hierarchical Build

Each level delegates down, collects artifacts, and cleans up:

```c
/* /src/build.c - root level */
int main() {
    build_subdir("libc");    // build, collect artifacts, cleanup
    build_subdir("cmd");
    return 0;
}

/* /src/cmd/build.c - proxy for all commands */
int main() {
    build_subdir("cc");
    build_subdir("as");
    build_subdir("ld");
    build_subdir("cat");
    build_subdir("ls");
    /* ... */
    return 0;
}

/* /src/cmd/cc/build.c - leaf: knows cc internals */
int main() {
    char *srcs[] = {"main", "tokenize", "parse", ...};
    for (int i = 0; srcs[i]; i++) {
        compile(srcs[i]);    // outputs to ./.build/obj/
    }
    link("build/bin/cc", srcs);  // outputs to ./build/bin/
    return 0;
}
```

`build_subdir()` handles: run build → move artifacts up → full cleanup of child dirs.
Child directories are left clean with no `.build/` or `build/` traces.

Simple commands (cat, ls, etc.) have no `build.c` - `/bin/build` uses fallback.

### .mkfsignore

Simple exclusion list for syncing source to `build/src/`:

```
build
.build
.git
disk.img
docsearch.db
.vscode
.idea
```

Format: one entry per line, prefix matching. No globs needed since artifacts
are contained in `.build/` and `build/`.

### Source Sync

Source sync is handled by `mkfs` with the `-m source:target` option:

```bash
mkfs disk.img -s 8192 -m .:src
```

This replaces the current explicit file listing in the Makefile. The tool:
1. Walks the source tree recursively
2. Skips entries matching `.mkfsignore` prefixes
3. Creates directories and copies files to the target path on the image

On slopix, a separate `sync` program can copy `/src` to `/src/build/src/`
using the same `.mkfsignore` logic.

### Build Flow

#### On Host (Cross-Compilation)

Host build has three distinct steps:

```
Step 1: Build the build tool
   cc -I lib cmd/build/main.c -o .bin/build
   → uses system compiler and libc
   → outputs to .bin/ (non-default location)

Step 2: Build cross-toolchain
   for tool in cc as ld ar; do
       .bin/build --prefix=.bin cmd/$tool
   done
   → uses system compiler and libc (CC=cc, LD=cc by default)
   → outputs to .bin/ via --prefix
   → .bin/cc, .bin/as, .bin/ld are native binaries that emit aarch64

Step 3: Build userspace (cross-compile)
   BUILD=.bin/build \
   CC=.bin/cc AS=.bin/as LD=.bin/ld AR=.bin/ar \
   INCLUDE_PATH=libc/include LIB_PATH=build/lib \
   .bin/build
   → .bin/build compiles root build.c with system cc → .build/build
   → .build/build inherits env, uses cross-toolchain
   → libc/build.c builds libc.a → build/lib/libc.a
   → cmd/build.c builds all commands → build/bin/*
   → final build/ contains everything for disk image

Step 4: Create disk image
   .bin/mkfs disk.img -m .build/out:/ -m .:src ...
   → copies build/bin/* to /bin/
   → copies build/lib/* to /lib/
   → syncs source to /src/

Step 5: Build kernel
   aarch64-elf-gcc compiles kernel/ → kernel.bin
   → requires GCC (inline asm, linker scripts)
```

**Key insight**: build.c files are always compiled with the system compiler
(native binaries), but they use `$CC`/`$AS`/`$LD` from environment to compile
the actual source files. This allows the same build.c to work for both host
bootstrap (Step 2) and cross-compilation (Step 3).

#### On Slopix (Self-Hosted)

```
1. Build everything
   cd /src && /bin/build
   → libc: builds → artifacts bubble to /src/build/
   → cmd: each subdir builds → artifacts bubble up
   → final /src/build/ contains all binaries

2. Sync source
   Copy /src to /src/build/src/ (respecting .mkfsignore)

3. Install
   Copy /src/build/* to /
```

The self-hosted flow is simpler: just `/bin/build` at root.

### Configuration

Toolchain and paths are configurable via environment variables, with defaults
matching slopix filesystem layout:

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `/bin/cc` | C compiler |
| `AS` | `/bin/as` | Assembler |
| `LD` | `/bin/ld` | Linker |
| `AR` | `/bin/ar` | Archive tool |
| `BUILD` | `/bin/build` | Build tool (for build_subdir) |
| `BUILD_PREFIX` | `build/bin` | Output directory for binaries |
| `INCLUDE_PATH` | `/src/libc/include` | Header search path (for compiled programs) |
| `BUILD_INCLUDE` | `/src/lib` | Build.h search path (for build.c files) |
| `LIB_PATH` | `/lib` | Library search path (for libc.a) |

Environment variables use standard Unix inheritance. When you run
`CC=/custom/cc /bin/build`, the custom CC is visible to all child processes
including compiled build.c programs (via `getenv()`).

**Host bootstrap (Step 2)**: Use `.bin/build --prefix=.bin` to build host tools.
The `--prefix` sets `BUILD_PREFIX=.bin`. Default `CC=cc` and `LD=cc` use system
compiler. Empty `LIB_PATH` (default on host) means system libc is used implicitly.

**Cross-compilation (Step 3)**: Use `CC=.bin/cc AS=.bin/as LD=.bin/ld ...` to
cross-compile aarch64 binaries using the host-built toolchain.

Userspace is compiled with `cmd/cc` (the slopix C compiler):
- On host: `.bin/cc` runs on macOS/Linux, produces aarch64 code
- On slopix: `/bin/cc` runs natively, produces aarch64 code
- Same compiler, same flags, same output

**Note on .S files:** The libc assembly files (`crt0.S`, `syscall.S`) do not
use C preprocessor directives. They can be assembled directly without
preprocessing, so no separate CPP tool is needed.

### Kernel Build

The kernel stays separate from the build system:
- Uses `aarch64-elf-gcc` (requires inline asm, linker scripts)
- Built via Makefile, not build.c
- No `/bin/build` wrapper (slopix has no `make`)

When slopix toolchain catches up (inline asm, linker scripts, etc.),
kernel can migrate to `/bin/build` too.

### build.h - Shared Utilities

Location: `lib/build.h` (project root) or `/src/lib/build.h` (on slopix disk)

**Include path handling:**
- Build.c files use: `#include "build.h"` (portable, no absolute paths)
- On slopix: `/bin/build` passes `-I/src/lib` when compiling build.c
- On host: `.bin/build` passes `-I$PROJECT_ROOT/lib`

Minimal nob-style header for build programs:

```c
#ifndef BUILD_H
#define BUILD_H

/* Dynamic string array for building commands */
typedef struct {
    const char **items;
    int count;
    int cap;
} Cmd;

void cmd_append(Cmd *c, ...);      /* NULL-terminated varargs */
void cmd_reset(Cmd *c);
int  cmd_run(Cmd *c);              /* fork/exec/wait, returns 0 on success */

/* Subdir builds with artifact collection and full cleanup */
int  build_subdir(const char *dir);
    /* 1. cd dir && /bin/build
       2. move dir/build/* → ./build/*
       3. rm -r dir/build/ dir/.build/
       Returns 0 on success, propagates child failures */

/* Leaf build helpers */
int  compile(const char *src);       /* cc -S → .build/obj/, as -o → .build/obj/ */
int  assemble(const char *src);      /* as -o .build/obj/<src>.o <src>.S */
int  link_objs(const char *out, const char **objs);    /* ld -o out objs... [libc.a] */
int  archive_objs(const char *out, const char **objs); /* ar rcs out objs... */

/* Note: link_objs() appends $LIB_PATH/libc.a only if LIB_PATH is set.
 * When LIB_PATH is empty (host bootstrap), system libc is used implicitly
 * via LD=gcc. This allows the same build.c to work for both host and target. */

/* File operations */
int  mkdir_p(const char *path);
int  move_recursive(const char *src, const char *dst);
int  remove_recursive(const char *path);
int  file_exists(const char *path);

/* Logging */
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif

#ifdef BUILD_IMPLEMENTATION
/* Implementation uses fork/exec/wait.
 *
 * Note: slopix exec() takes a single command string, not argv array.
 * The cmd_run() implementation builds a command string from Cmd items.
 * This works but doesn't handle arguments with spaces. Future improvement:
 * add proper quoting or change kernel exec to accept argv directly.
 */
#endif
```

### Error Handling

Build programs follow these conventions:

1. Return non-zero on any failure
2. Leave `.build/` intact for debugging (contains intermediate files)
3. Do NOT create `build/` entries until all steps succeed
4. `build_subdir()` propagates child failures immediately (early return)

If a build fails mid-way, the `.build/` directory shows what was completed.
Run `/bin/build clean` to reset and try again.

### Example: Leaf Build (Component)

```c
/* /src/cmd/cc/build.c - knows how to build cc */
#define BUILD_IMPLEMENTATION
#include "build.h"

static const char *srcs[] = {
    "main", "tokenize", "preprocess", "parse",
    "type", "codegen", "unicode", "strings", "hashmap",
    NULL
};

int main() {
    mkdir_p(".build/obj");
    mkdir_p("build/bin");

    for (int i = 0; srcs[i]; i++) {
        if (compile(srcs[i]) != 0) return 1;  /* → .build/obj/*.o */
    }
    return link_objs("build/bin/cc", srcs);    /* → build/bin/cc */
}
```

### Example: Proxy Build (Aggregator)

```c
/* /src/cmd/build.c - iterates subdirectories, collects artifacts */
#define BUILD_IMPLEMENTATION
#include "build.h"

int main() {
    /* Each build_subdir():
       - runs /bin/build in subdir
       - moves subdir/build/* → ./build/*
       - removes subdir/.build/ and subdir/build/ completely */

    if (build_subdir("cc") != 0) return 1;
    if (build_subdir("as") != 0) return 1;
    if (build_subdir("ld") != 0) return 1;
    if (build_subdir("ar") != 0) return 1;
    if (build_subdir("cat") != 0) return 1;
    if (build_subdir("ls") != 0) return 1;
    /* ... */

    return 0;
}
```

### Example: Library Build (libc)

```c
/* /src/libc/build.c - builds libc.a */
#define BUILD_IMPLEMENTATION
#include "build.h"

static const char *c_srcs[] = {
    "ctype", "errno", "libgen", "malloc", "stdio",
    "stdio_file", "stdlib", "string", "test", "time",
    NULL
};

static const char *asm_srcs[] = {
    "crt0", "syscall",
    NULL
};

int main() {
    mkdir_p(".build/obj");
    mkdir_p("build/lib");

    /* Compile C sources */
    for (int i = 0; c_srcs[i]; i++) {
        if (compile(c_srcs[i]) != 0) return 1;
    }

    /* Assemble .S files directly (no preprocessing needed) */
    for (int i = 0; asm_srcs[i]; i++) {
        if (assemble(asm_srcs[i]) != 0) return 1;
    }

    /* Create archive from all object files */
    const char *all_objs[] = {
        "crt0", "syscall",
        "ctype", "errno", "libgen", "malloc", "stdio",
        "stdio_file", "stdlib", "string", "test", "time",
        NULL
    };
    return archive_objs("build/lib/libc.a", all_objs);
}
```

## Build Dependencies

```
.bin/build (system cc compiles cmd/build/main.c)
   ↓
.bin/{cc,as,ld,ar,mkfs,mkramfs} (.bin/build --prefix=.bin with system cc)
   ↓
build/lib/libc.a (cross-compile with CC=.bin/cc)
   ↓
build/bin/* (cross-compile with CC=.bin/cc, links libc.a)
```

Kernel is separate (host GCC for now).

**Isolated sub-builds**: When running a sub-build directly (e.g., `cd cmd/cc && /bin/build`),
dependencies are assumed to exist in default locations. If libc.a doesn't exist,
the build will fail. Run full build first to ensure dependencies are in place.

## Design Decisions

- **Testing**: Out of scope - Makefile handles QEMU and test execution
- **Subdirectory discovery**: Hardcoded lists - explicit, no surprises
- **Debug/Release builds**: Out of scope - single configuration for now
- **No platform detection**: Use command-line flags (`-I`, `-L`) not `#ifdef`
- **Always recompile build.c**: No caching until mtime support exists

## Migration Plan

### Phase 1: Directory Structure

1. Create `lib/` directory at project root
2. Create `.build/` and `build/` directories
3. Update `.gitignore` to include `.build/` and `build/`
4. Create `.mkfsignore`

**Verification:**
- [x] `lib/` directory exists
- [x] `.gitignore` updated
- [x] `.mkfsignore` created

### Phase 2: Consolidate Headers

1. Merge `cmd/cc/include/*.h` into `libc/include/`
2. Update any differing definitions (prefer libc versions)
3. Delete `cmd/cc/include/` directory
4. Update Makefiles to use single include path

**Verification:**
- [x] All headers in `libc/include/`
- [x] `cmd/cc/include/` deleted
- [x] `make test` passes

### Phase 3: Consolidate Sources

1. Make `cmd/cc/main.c` configurable (remove hardcoded paths)
2. Apply platform-independent fixes from tools/cc to cmd/cc
3. Move `tools/mkfs/` to `cmd/mkfs/`
4. Move `tools/mkramfs/` to `cmd/mkramfs/`
5. Update root Makefile to build host tools from cmd/
6. Remove `tools/` directory entirely

mkfs/mkramfs are regular programs - they get build.c files and are installed
to `/bin/` like everything else. They're also bootstrapped for host use.

**Verification:**
- [x] Host cc/as/ld build from cmd/ sources
- [x] Host mkfs/mkramfs build from cmd/
- [x] `tools/` directory removed
- [x] `make test` passes

### Phase 4: Build Infrastructure

1. Create `lib/build.h` with nob-style utilities
2. Create `cmd/build/main.c` - the `/bin/build` tool
3. Add `-m source:target` option to mkfs
4. Add `--prefix` option to build tool

**Verification:**
- [x] `cc -I lib cmd/build/main.c -o .bin/build` works
- [x] `.bin/build --prefix=.bin cmd/cc` builds host cc
- [x] `mkfs -m` works

### Phase 5: Add build.c Files

1. Create `libc/build.c`
2. Create build.c for multi-file programs: `cmd/cc/`, `cmd/as/`, `cmd/ld/`, `cmd/mkfs/`, etc.
3. Create `cmd/build.c` (proxy for all commands)
4. Create root `build.c`

**Verification:**
- [x] `.bin/build --prefix=.bin cmd/cc` produces `.bin/cc`
- [x] Cross-compile: `CC=.bin/cc ... .bin/build libc` produces `build/lib/libc.a`
- [x] Cross-compile: `CC=.bin/cc ... .bin/build cmd` builds all commands
- [x] Cross-compile: `CC=.bin/cc ... .bin/build` at root builds everything

### Phase 6: Migrate and Verify

1. Switch Makefile to use new build system for userspace
2. Verify disk.img creation works with -m
3. Remove old Makefiles from libc/, cmd/, cmd/*/ subdirectories
4. Final verification on slopix

**Verification:**
- [ ] `make` builds complete system
- [ ] `make test` passes
- [ ] Boot slopix, run `/bin/build` in `/src/cmd/cc`
- [ ] Resulting `/tmp/cc.elf` works
- [ ] Old Makefiles removed

## Future Work

Items deferred to BACKLOG.md:

- **mtime-based rebuild**: Skip recompilation when sources unchanged
- **Self-rebuilding build.c**: NOB_GO_REBUILD_URSELF pattern
- **exec() compliance**: Proper argv handling or quoting for arguments with spaces
- **Parallel builds**: Multiple compilations at once

## References

- [nob.h](https://github.com/tsoding/nob.h) - Header-only C build system
- [SELF_HOSTING.md](SELF_HOSTING.md) - Self-hosting roadmap
- [BACKLOG.md](BACKLOG.md) - Deferred work items
