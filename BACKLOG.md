# Slopix Backlog

## Incremental Builds

Skip recompilation when source hasn't changed. Use mtime comparison like nob.h.

**Blocked by:** `st_mtime` not populated in stat()

## Build Self-Rebuild

nob's "Go Rebuild Urself" pattern - build program detects if its own source
changed and recompiles itself before proceeding.

**Blocked by:** Requires mtime comparison

## Kernel Self-Hosting

Build kernel with slopix toolchain instead of host GCC.

**Blocked by:** Toolchain gaps documented in SELF_HOSTING.md:
- Inline assembly support in cc
- Assembler macros (.macro/.endm)
- Assembler directives (.equ, .fill, .balign)
- Linker scripts
- Binary output (objcopy or ld --oformat=binary)
