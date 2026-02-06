# Slopix Backlog

## Incremental Builds

Skip recompilation when source hasn't changed. Use mtime comparison like nob.h.

**Blocked by:** `st_mtime` not populated in stat()

## Build Self-Rebuild

nob's "Go Rebuild Urself" pattern - build program detects if its own source
changed and recompiles itself before proceeding.

**Blocked by:** Requires mtime comparison

