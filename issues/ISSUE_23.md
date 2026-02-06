# Code Duplication in ELF Loading Functions

## Summary
Two nearly identical ELF loading functions exist in `kernel/elf.c` that differ only in how they read segment data. This duplication creates maintenance burden and risk of divergence in core bootstrap logic.

## Severity
**High**

## Location
**File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/elf.c`
- `elf_load()`: lines 7-84 (78 lines)
- `elf_load_from_inode()`: lines 86-184 (99 lines)

## Problem Description

### Current State
Two functions implement nearly identical ELF segment loading logic:

1. **`elf_load()`** - loads from in-memory buffer
   - Reads ELF header from `const char *data` pointer
   - Uses `memcpy()` to copy segment data from buffer to allocated pages
   - Direct pointer arithmetic for file offsets

2. **`elf_load_from_inode()`** - loads from filesystem inode
   - Reads ELF header via `fs_readi()` into stack-allocated `Elf64_Ehdr`
   - Uses `fs_readi()` to read segment data into allocated pages
   - Includes additional error handling for file reads and memory cleanup

### Identical Code Blocks
Lines 7-84 vs 86-184 share ~85-90% identical logic:

| Aspect | Lines in elf_load | Lines in elf_load_from_inode | Status |
|--------|-------------------|------------------------------|--------|
| ELF header validation | 10-25 | 94-106 | Identical |
| Extract segment parameters | 35-40 | 129-134 | Identical |
| VA alignment computation | 42-43 | 136-138 | Identical |
| Page loop structure | 45-74 | 140-174 | Identical |
| Page offset calculation | 61-64 | 157-160 | Identical |
| vmm_map_page() call | 70-73 | 169-173 | Identical |
| max_addr tracking | 76-78 | 176-178 | Identical |
| Entry/brk assignment | 81-82 | 181-182 | Identical |

**Only difference:**
- Lines 65-67 (elf_load): `memcpy((char *)page + page_offset, data + file_offset, copy_len);`
- Lines 162-166 (elf_load_from_inode): `fs_readi(ip, (char *)page + page_offset, file_offset, copy_len)` with error handling

## Risk of Divergence

Bootstrap code is security and correctness critical. Duplication creates risks:

1. **Bug fix isolation**: A bug fix applied to one function may not propagate to the other
   - Example: If a VA boundary condition bug is fixed in `elf_load()`, `elf_load_from_inode()` could remain vulnerable

2. **Feature inconsistency**: Future improvements (e.g., segment permission validation, address space bounds checking) risk being applied only partially

3. **Maintenance burden**: Code review requires checking both implementations

4. **Memory leak potential**: `elf_load_from_inode()` includes `pmm_free()` on error (lines 164, 171), but this was added after initial implementation—duplication means this pattern could be missed if similar cleanup is needed elsewhere

## Test Recommendations

After refactoring:

1. **Unit test both code paths**: Ensure refactored code works for both buffer-based and inode-based loading
   - Load ELF from buffer: verify segment mapping and entry/brk values
   - Load ELF from inode: verify same with filesystem reads

2. **Regression test**: Verify existing userspace binary still loads and executes correctly (both code paths if both are tested)

3. **Edge cases**:
   - Misaligned segment starts and ends
   - Segments spanning multiple pages
   - Gaps between segments
   - memsz > filesz (BSS segments)
   - Invalid ELF headers (magic, machine type, file type)
   - File read failures (inode path)

## Fixing Recommendations

### Approach: Extract Common Segment Loader

Create a callback-based helper function that abstracts the data source:

```c
// Reader callback function type
// Returns: bytes read, or -1 on error
typedef int (*elf_reader)(void *ctx, unsigned long offset, char *buf, unsigned long len);

// Common ELF validation and segment loading
int elf_load_internal(
    Elf64_Ehdr *ehdr,
    Elf64_Phdr *phdr,
    int phnum,
    pte_t *pagetable,
    unsigned long *entry,
    unsigned long *brk,
    elf_reader reader,
    void *reader_ctx
);
```

### Implementation Steps

1. **Create `elf_load_internal()`**:
   - Move ELF header validation from both functions
   - Extract segment loop logic (lines 30-79 / 124-179)
   - Replace data reading with `reader()` callback
   - Handle both sync (buffer) and async (filesystem) reads uniformly

2. **Update `elf_load()`**:
   - Validate header
   - Call `elf_load_internal()` with buffer reader callback
   - Buffer reader: `memcpy(buf, data + offset, len); return len;`

3. **Update `elf_load_from_inode()`**:
   - Read header and program headers via `fs_readi()`
   - Call `elf_load_internal()` with filesystem reader callback
   - Filesystem reader: `return fs_readi(ip, buf, offset, len);`
   - Keep error handling and cleanup at this level

### Benefits

- **Single source of truth**: Segment loading logic centralized in one function
- **Reduced duplication**: ~60 lines of identical code eliminated
- **Type safety**: Callback mechanism preserves flexibility without void pointers if needed
- **Testability**: `elf_load_internal()` can be unit tested with stub readers
- **Maintainability**: Bug fixes and improvements happen once

### Migration Path

- Use weak refactoring: Keep both existing function signatures
- No changes needed to callers
- Both functions tested against existing test cases
- Could be done incrementally if needed

## References

- ELF specification for 64-bit ARM (AArch64) defines segment layout and loading semantics
- Current code: `/Users/davidklassen/work/davidklassen/slopix/kernel/elf.c` and `/Users/davidklassen/work/davidklassen/slopix/kernel/elf.h`
