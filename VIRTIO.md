# Virtio Block Driver Roadmap

Implementation plan for virtio-blk driver and block cache layer using legacy MMIO interface.

**Parent**: [ROADMAP.md](ROADMAP.md) Milestone 10: Block Device

## Overview

The virtio-blk driver provides block device access to a QEMU virtual disk. This enables the filesystem to read and write persistent storage.

Target: Legacy virtio-mmio (version 1) interface. QEMU assigns the first device
to slot 31 at 0x0a00_3e00 (slots are filled in reverse order from 0x0a00_0000 base).

## Architecture

```
+------------------+
|   Filesystem     |  bread(), bwrite()
+------------------+
        |
+------------------+
|   Block Cache    |  buf.h, bio.c
+------------------+
        |
+------------------+
|   Virtio Driver  |  virtio.c/h
+------------------+
        |
    MMIO + IRQ
        |
+------------------+
|   QEMU virtio    |
+------------------+
```

## Milestones

### M1: Device Discovery and Reset ✓

Probe the virtio-mmio region and verify we have a block device.

- [x] Define MMIO register offsets in virtio.h
- [x] Implement `virtio_probe()`:
  - Read and verify MagicValue (0x74726976)
  - Read and verify Version (1 = legacy)
  - Read and verify DeviceID (2 = block)
  - Read VendorID for logging
- [x] Implement `virtio_reset()`: write 0 to Status
- [x] Add virtio_init() call from kernel_main()
- [x] Add `test_virtio.c` with `virtio` suite (4 read-only tests)

**Exit criteria**: Boot prints "virtio-blk: found block device". All discovery tests pass.

### M2: Feature Negotiation ✓

Negotiate device features and transition through status bits.

- [x] Define feature bits (VIRTIO_BLK_F_*) - deferred, accepting defaults
- [x] Define status bits (ACKNOWLEDGE, DRIVER, DRIVER_OK)
- [x] Implement feature negotiation:
  - Read DeviceFeatures
  - Mask to supported features (initially: none, accept defaults)
  - Write DriverFeatures
- [x] Set ACKNOWLEDGE and DRIVER status bits
- [x] Read block device config (capacity at offset 0x100)
- [x] Add `virtio_features` suite (2 read-only tests)

**Exit criteria**: Boot prints "virtio-blk: capacity = N sectors". All feature tests pass.

### M3: Virtqueue Setup ✓

Allocate and configure the request virtqueue.

- [x] Define virtqueue structures:
  - `struct virtq_desc` (16 bytes)
  - `struct virtq_avail` (6 + 2*N bytes)
  - `struct virtq_used` (6 + 8*N bytes)
- [x] Choose queue size (8 is sufficient for simple use)
- [x] Allocate contiguous pages for virtqueue
- [x] Zero the memory
- [x] Configure queue registers:
  - Write GuestPageSize (4096)
  - Write QueueSel (0)
  - Verify QueueNumMax >= our size
  - Write QueueNum
  - Write QueueAlign (4096)
  - Write QueuePFN
- [x] Set DRIVER_OK status bit
- [x] Initialize descriptor free list
- [x] Add `virtio_queue` suite (4 tests)

**Exit criteria**: Device status remains DRIVER_OK after setup. All queue tests pass.

### M4: Synchronous Block Read (Polling)

Implement blocking read using polling (no interrupts).

- [ ] Define block request header (`struct virtio_blk_outhdr`)
- [ ] Implement `virtio_disk_rw(sector, buf, write)`:
  - Allocate 3 descriptors (header, data, status)
  - Fill header with type=IN, sector
  - Chain descriptors with NEXT flags
  - Set WRITE flag on data and status descriptors
  - Add head to available ring
  - Increment avail.idx
  - Memory barrier (DSB)
  - Write to QueueNotify
  - Poll used.idx until it advances
  - Check status byte
  - Free descriptors
- [ ] Implement `virtio_disk_read(sector, buf)`
- [ ] Add `virtio_read` suite (3 tests)

**Exit criteria**: Can read superblock from disk. All read tests pass.

### M5: Synchronous Block Write (Polling)

Extend to support writes.

- [ ] Implement `virtio_disk_write(sector, buf)`:
  - Same as read but type=OUT
  - Data descriptor: no WRITE flag (device reads from buffer)
- [ ] Add `virtio_write` suite (2 tests)

**Exit criteria**: Read-after-write returns correct data. All write tests pass.

### M6: Interrupt-Driven I/O

Replace polling with interrupt handling for efficiency.

- [ ] Enable virtio interrupt in GIC (INTID 48)
- [ ] Implement `virtio_intr()` handler:
  - Read InterruptStatus
  - Write InterruptACK
  - Process used ring entries
  - Wake waiting processes
- [ ] Add sleep channel to virtio driver
- [ ] Modify `virtio_disk_rw()` to sleep instead of poll
- [ ] Track in-flight requests (buffer pointer per descriptor)
- [ ] Add `virtio_intr` suite (2 kernel tests)
- [ ] Add `disk_concurrent` suite (1 userspace test) if disk syscalls exist

**Exit criteria**: Processes sleep during I/O, wake on completion. All interrupt tests pass.

### M7: Block Cache Integration

Connect virtio driver to the buffer cache layer.

- [ ] Implement `buf.h` buffer structure
- [ ] Implement `bread(dev, blockno)`:
  - Check cache for block
  - If miss, allocate buffer and call virtio_disk_read
  - Return buffer (caller must release)
- [ ] Implement `bwrite(buf)`:
  - Call virtio_disk_write
  - Mark buffer clean
- [ ] Implement `brelse(buf)`: release buffer to cache
- [ ] LRU replacement policy
- [ ] Add `test_bio.c` with `bio` suite (6 tests)

**Exit criteria**: Filesystem can use bread/bwrite interface. All bio tests pass.

### M8: Error Handling

Add robustness for device errors.

- [ ] Check status byte after every operation
- [ ] Handle IOERR: retry or return error
- [ ] Handle UNSUPP: panic or return error
- [ ] Detect device reset (status register clears)
- [ ] Timeout detection for hung device
- [ ] Add `virtio_errors` suite (2 tests)

**Exit criteria**: Driver recovers from or reports errors gracefully. All error tests pass.

## Data Structures Summary

```c
// MMIO registers (offsets from device base, e.g., 0x0a00_3e00 for slot 31)
#define VIRTIO_MMIO_MAGIC           0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_ALIGN     0x03c
#define VIRTIO_MMIO_QUEUE_PFN       0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_CONFIG          0x100

// Status bits
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4

// Descriptor flags
#define VIRTQ_DESC_F_NEXT           1
#define VIRTQ_DESC_F_WRITE          2

// Block request types
#define VIRTIO_BLK_T_IN             0  // read
#define VIRTIO_BLK_T_OUT            1  // write

// Block status values
#define VIRTIO_BLK_S_OK             0
#define VIRTIO_BLK_S_IOERR          1
#define VIRTIO_BLK_S_UNSUPP         2

// Virtqueue descriptor
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

// Available ring
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};

// Used ring element
struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

// Used ring
struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
};

// Block request header
struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};
```

## Testing Strategy

Tests use the kernel test framework (`kernel/tests/test.h`). Create `kernel/tests/test_virtio.c` and `kernel/tests/test_bio.c`.

### M1: Device Discovery (kernel tests, read-only)

```c
TEST_SUITE(virtio) {
    RUN_TEST(virtio_magic_value);      // == 0x74726976
    RUN_TEST(virtio_version_legacy);   // == 1
    RUN_TEST(virtio_device_id_block);  // == 2
    RUN_TEST(virtio_vendor_id_qemu);   // == 0x554d4551
}
```

### M2: Feature Negotiation (kernel tests, read-only)

```c
TEST_SUITE(virtio_features) {
    RUN_TEST(virtio_status_initialized);  // status == 3 after init
    RUN_TEST(virtio_config_capacity);     // capacity > 0
}
```

### M3: Virtqueue Setup (kernel tests)

```c
TEST_SUITE(virtio_queue) {
    RUN_TEST(virtio_queue_num_max);       // QueueNumMax > 0
    RUN_TEST(virtio_queue_alloc);         // allocation succeeds
    RUN_TEST(virtio_queue_pfn_written);   // QueuePFN != 0 after setup
    RUN_TEST(virtio_status_driver_ok);    // status == 7 after init
}
```

### M4: Synchronous Read (kernel tests)

```c
TEST_SUITE(virtio_read) {
    RUN_TEST(virtio_read_sector_zero);    // read succeeds, status == 0
    RUN_TEST(virtio_read_returns_data);   // buffer not all zeros
    RUN_TEST(virtio_read_multiple);       // read sectors 0, 1, 2
}
```

### M5: Synchronous Write (kernel tests)

```c
TEST_SUITE(virtio_write) {
    RUN_TEST(virtio_write_read_verify);   // write pattern, read back, compare
    RUN_TEST(virtio_write_multiple);      // write to multiple sectors
}
```

**Note**: Write tests should use a scratch sector (not sector 0) to avoid corrupting filesystem.

### M6: Interrupt-Driven I/O (kernel + userspace tests)

Kernel tests:
```c
TEST_SUITE(virtio_intr) {
    RUN_TEST(virtio_intr_enabled);        // GIC interrupt 48 enabled
    RUN_TEST(virtio_intr_fires);          // interrupt received after I/O
}
```

Userspace tests (if disk syscalls exist):
```c
TEST_SUITE(disk_concurrent) {
    RUN_TEST(disk_fork_read);             // parent and child read different sectors
}
```

### M7: Block Cache (kernel tests)

```c
TEST_SUITE(bio) {
    RUN_TEST(bread_returns_buffer);       // bread() != NULL
    RUN_TEST(brelse_frees_buffer);        // buffer returned to pool
    RUN_TEST(bread_cache_hit);            // second read same block is cached
    RUN_TEST(bread_cache_miss);           // read new block fetches from disk
    RUN_TEST(bwrite_marks_dirty);         // buffer marked dirty after write
    RUN_TEST(cache_lru_eviction);         // oldest buffer evicted when full
}
```

### M8: Error Handling (kernel tests)

```c
TEST_SUITE(virtio_errors) {
    RUN_TEST(virtio_read_bad_sector);     // returns error for invalid sector
    RUN_TEST(virtio_status_check);        // detects IOERR status byte
}
```

**Note**: Some error conditions (device reset, timeout) are hard to trigger in automated tests.

### Test Summary

| Milestone | Suite | Tests | Automated |
|-----------|-------|-------|-----------|
| M1 | virtio | 4 | ✓ kernel (read-only) |
| M2 | virtio_features | 2 | ✓ kernel (read-only) |
| M3 | virtio_queue | 4 | ✓ kernel |
| M4 | virtio_read | 3 | ✓ kernel |
| M5 | virtio_write | 2 | ✓ kernel |
| M6 | virtio_intr | 2 | ✓ kernel |
| M6 | disk_concurrent | 1 | ✓ userspace |
| M7 | bio | 6 | ✓ kernel |
| M8 | virtio_errors | 2 | ✓ kernel |

**Total**: 26 automated tests across 9 test suites.

**Note**: Tests are read-only where possible to ensure identical system state in test and normal modes.

## References

- DESIGN.md: Virtio Block Device section
- [OASIS VIRTIO Spec v1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html)
- [Linux virtio_mmio.c](https://github.com/torvalds/linux/blob/master/drivers/virtio/virtio_mmio.c)
- [xv6-riscv virtio_disk.c](https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/virtio_disk.c) (modern interface, but good reference for structure)
