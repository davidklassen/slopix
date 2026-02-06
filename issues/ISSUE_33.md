# ISSUE_33: Test Suite Violates Read-Only Test Principle

## Title
Kernel tests violate read-only test design principle by modifying system state

## Severity
**Critical**

Multiple test suites directly violate the read-only test principle defined in CLAUDE.md, compromising test integrity and introducing risk of state pollution between tests.

## Affected Files

### test_bio.c
- **Line 28**: `b1->data[0] = 0xAB;` - Writes to buffer cache data
- **Line 50**: Loop writes `(unsigned char)(i & 0xFF)` to all BSIZE bytes - Modifies disk buffer
- **Line 52**: `bio_write(b);` - Writes data to disk device
- **Line 56**: `b->valid = 0;` - Invalidates cache state (modifies internal state)
- **Line 72**: `first->data[0] = 0x42;` - Writes to buffer cache data

### test_sync.c
- **Line 17**: `spin_lock(&lk);` - Acquires lock, modifying lock state
- **Line 20**: `spin_unlock(&lk);` - Releases lock, modifying lock state
- **Line 29**: `enable_irq();` - Modifies CPU IRQ state
- **Line 32**: `spin_lock(&lk);` - Acquires lock (state change)
- **Line 35**: `spin_unlock(&lk);` - Releases lock (state change)
- **Line 56**: `spin_lock(&lk);` - Acquires lock (state change)
- **Line 69**: `sleep_lock(&lk);` - Acquires sleep lock (state change)
- **Line 71**: `sleep_unlock(&lk);` - Releases sleep lock (state change)
- **Line 80**: `spin_lock(&lk);` - Acquires lock (state change)
- **Line 82**: `spin_unlock(&lk);` - Releases lock (state change)
- **Line 89**: `sleep_lock(&lk);` - Acquires sleep lock (state change)
- **Line 91**: `sleep_unlock(&lk);` - Releases sleep lock (state change)

### test_tlb.c
- **Line 12**: `pte_t *pt = vmm_create();` - Allocates memory
- **Line 15-19**: `pmm_alloc()` calls - Allocate physical pages (memory management)
- **Line 21**: `vmm_map_page(pt, TEST_VA, pa1, 1, 0);` - Maps virtual page
- **Line 28**: `write_ttbr0_el1(VA_TO_PA(pt));` - Switches page tables
- **Line 29**: `tlbi_vmalle1();` - Invalidates TLB (CPU state change)
- **Line 33**: `*p = 0xAB;` - Writes to memory via virtual address
- **Line 37**: `vmm_unmap_page(pt, TEST_VA, &unmapped_pa);` - Unmaps page
- **Line 42**: `vmm_map_page(pt, TEST_VA, pa2, 1, 0);` - Maps new page
- **Line 46**: `memset(PA_TO_VA(pa2), 0, PAGE_SIZE);` - Writes to physical memory
- **Line 53**: `write_ttbr0_el1(old_ttbr0);` - Restores page tables (late cleanup)
- **Line 54**: `tlbi_vmalle1();` - TLB invalidation
- **Line 58-59**: `vmm_free()` and `pmm_free()` - Deallocates memory (cleanup)

### test_virtio.c
- **Line 56**: `VIRTIO_REG(VIRTIO_MMIO_QUEUE_SEL) = 0;` - Writes to device register
- **Line 69**: `VIRTIO_REG(VIRTIO_MMIO_QUEUE_SEL) = 0;` - Writes to device register
- **Line 92, 98, 108-109**: `virtio_disk_read()` calls - Perform disk I/O operations
- **Line 129-130**: Loop writes `(unsigned char)(i & 0xFF)` to buffer
- **Line 131**: `virtio_disk_write(SCRATCH_SECTOR_1, write_buf);` - Writes sectors to disk
- **Line 135**: Loop clears write_buf with zeros - Modifies test buffer
- **Line 137**: `virtio_disk_read(SCRATCH_SECTOR_1, write_buf);` - Performs disk read
- **Line 152-154**: Loop writes sectors 100-102 to disk - Modifies disk state
- **Line 158**: `virtio_disk_read(sectors[s], write_buf);` - Performs disk read

## Description

The test framework is designed with read-only semantics to ensure identical CPU/device state between test and normal execution modes (CLAUDE.md, "Test Design" section):

> "Tests are **read-only** — they check that init functions set correct state... Tests must **not** write to registers, allocate memory, or change CPU/device state."

However, multiple test suites violate this principle by actively modifying system state:

1. **test_bio.c**: Writes buffer data, modifies cache validity flags, and performs actual disk writes
2. **test_sync.c**: Acquires/releases locks and modifies CPU IRQ state with `enable_irq()`
3. **test_tlb.c**: Allocates pages, switches page tables, maps/unmaps pages, and writes memory
4. **test_virtio.c**: Writes device registers and performs actual disk I/O (read/write sectors)

These tests perform the exact operations they're supposed to verify, leaving the kernel in a modified state that differs from what a normal boot sequence would establish.

## Risk Assessment

### High Risk
1. **State Pollution**: Subsequent tests or boot sequence may encounter state left by earlier tests
2. **Non-Determinism**: Test results become dependent on execution order and previous test outcomes
3. **Device State**: Disk sectors 100-102 are now modified, affecting subsequent disk operations
4. **Memory Leaks**: Page tables and physical pages allocated in test_tlb.c may not be properly freed
5. **Lock State Uncertainty**: Locks are acquired/released in tests without guarantee of final state
6. **TLB Corruption**: test_tlb.c modifies TTBR0 and TLB, relying on cleanup that may fail

### Critical Failures
- If test_tlb.c cleanup fails (Ctrl+C or crash), kernel continues with wrong TTBR0
- If test_sync.c leaves IRQs disabled or locks held, subsequent code breaks
- If test_virtio.c corrupts disk, filesystem operations during userspace fail
- If test_bio.c corrupts buffer cache, disk reads return wrong data

## Test Recommendations

### Correct Approach (Read-Only)
Tests should only *observe* state, not modify it. For example:

- **test_bio.c**: Remove all `bio_write()` calls. Instead, read known-good blocks and verify their content
- **test_sync.c**: Create local, uninitialized lock variables and call init functions. Only call acquisition functions on fresh locks created in the test (not system locks)
- **test_tlb.c**: Remove page table switching, memory writes, and allocations. Instead, read system registers to verify TLB invalidation was called during `vmm_unmap_page()` inside `vmm_init()`
- **test_virtio.c**: Remove all `virtio_disk_write()` calls and register writes. Read device registers to verify initialization state set by `virtio_init()`

### Implementation Strategy
1. Identify what state each test needs to verify
2. Determine if that state was set by the init function (good candidate for read-only test)
3. If the test requires modification to verify (e.g., "TLB invalidation works"), redesign to verify observable side effects instead
4. Use scratch/test-specific state only if absolutely necessary, with guaranteed cleanup

## Fixing Recommendations

### Phase 1: Immediate Mitigations
1. Add documentation to each affected test explaining it's temporary/non-compliant
2. Isolate tests to use separate scratch areas (non-overlapping disk sectors, isolated memory)
3. Add explicit cleanup sections that restore state (use `volatile` to prevent optimization)

### Phase 2: Refactor to Read-Only
1. **test_bio.c**: Remove write tests; verify cache initialization and reference counting through reads
2. **test_sync.c**: Restructure to test only initialization state, not acquisition/release
3. **test_tlb.c**: Verify TLB state changes through side-effect observation (page fault handlers, timing, etc.) instead of direct modification
4. **test_virtio.c**: Verify device initialization state through register reads only; remove sector writes

### Phase 3: Validation
1. Run `make test` with all changes
2. Verify that `make run` produces identical boot sequence with and without tests
3. Add post-test state verification to confirm no pollution occurred
