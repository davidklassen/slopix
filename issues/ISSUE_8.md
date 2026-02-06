# ISSUE_8: Missing DSB Barrier in virtio_intr() After MMIO Interrupt Acknowledgment

## Severity
**Critical**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/virtio.c`
- **Lines:** 147-149

## Description

In the `virtio_intr()` function, after acknowledging the interrupt by writing to the MMIO register `VIRTIO_MMIO_INTERRUPT_ACK` (line 147), the code immediately reads `used->idx` (line 149, normal cached memory) without an intervening DSB (Data Synchronization Barrier) instruction.

```c
void virtio_intr(void) {
	unsigned int status = VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_STATUS);
	VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_ACK) = status;  // MMIO write

	while (last_used_idx != used->idx) {              // Memory read WITHOUT barrier
		last_used_idx++;
	}

	proc_wakeup(&virtio_disk_chan);
}
```

### The Memory Ordering Problem

1. The MMIO write to `VIRTIO_MMIO_INTERRUPT_ACK` signals to the device that the interrupt has been acknowledged
2. The device may then update the used ring buffer (specifically `used->idx`)
3. **Without a DSB barrier**, the CPU can read `used->idx` before the device's writes have completed, returning stale data
4. This violates the ARM AArch64 memory ordering model where device-generated writes to normal memory are not guaranteed to be visible without proper synchronization

### Comparison with Correct Pattern

In `virtio_disk_rw()` (lines 196-202), the code correctly uses DSB barriers around shared memory access:

```c
avail->ring[avail->idx % VIRTIO_QUEUE_SIZE] = idx[0];
dsb();                                           // Barrier after descriptor write
avail->idx++;
dsb();                                           // Barrier before device notification
VIRTIO_REG(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;       // Notify device
```

Later, when reading the used ring (line 206), the loop waits for the device to write to `used->idx`:

```c
while (used->idx == last_used) {  // Waiting for device update
	// ...
}
```

While this loop may eventually see the update when a timeout check or `proc_wait_timeout_nointr()` completes, it still lacks explicit synchronization with device writes.

The `virtio_intr()` function must use a similar DSB pattern after the MMIO acknowledgment to ensure visibility of the device's subsequent writes to the used ring.

## How to Reproduce

This is a subtle race condition that manifests under specific timing conditions:

1. Device generates interrupt and updates used ring while acknowledged
2. CPU reads `used->idx` before the device update is visible
3. `last_used_idx` remains equal to `used->idx`, loop exits without processing completed requests
4. Disk requests hang or appear to timeout

The bug is most likely to occur when:
- High interrupt load from the device (multiple rapidly completed requests)
- CPU caches miss on the used ring memory (though caching policies may mask the issue)
- Specific CPU/device timing interactions on QEMU

## Test Recommendations

### Automated Test (if feasible)
- Create a high-throughput disk I/O test that rapidly submits and completes disk requests
- Verify that all requests complete successfully without hangs
- Run under stress to increase timing variability and expose race conditions
- Example: Sequential reads/writes of 100+ sectors in tight loops

### Manual Testing
1. Build kernel with `make tidy && make test`
2. Run interactive tests with multiple back-to-back disk I/O operations
3. Monitor for stuck processes or timeouts on virtio disk operations
4. Compare behavior before and after fix

## Fixing Recommendations

### Specific Code Change

Add a DSB barrier immediately after acknowledging the interrupt but before reading the used ring:

**Current code (lines 145-154):**
```c
void virtio_intr(void) {
	unsigned int status = VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_STATUS);
	VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_ACK) = status;

	while (last_used_idx != used->idx) {
		last_used_idx++;
	}

	proc_wakeup(&virtio_disk_chan);
}
```

**Fixed code:**
```c
void virtio_intr(void) {
	unsigned int status = VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_STATUS);
	VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_ACK) = status;
	dsb();  // Ensure device's writes to used ring are visible

	while (last_used_idx != used->idx) {
		last_used_idx++;
	}

	proc_wakeup(&virtio_disk_chan);
}
```

### Rationale

The DSB (Data Synchronization Barrier) instruction:
- Completes all prior memory operations (including the MMIO write)
- Prevents the CPU from speculating ahead and reading `used->idx` before the device update
- Is defined in `kernel/cpu.h` as `asm volatile("dsb sy")` — a full system-wide barrier appropriate for synchronizing with device memory

This matches the memory barrier pattern used elsewhere in the codebase and ensures correct Virtio queue semantics.
