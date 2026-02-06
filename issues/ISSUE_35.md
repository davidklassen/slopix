# ISSUE_35: Test Hardcodes Disk Image Magic Bytes

## Severity
High

## Location
**File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/tests/test_virtio.c`
**Lines:** 97-105

## Description

The `virtio_read_returns_data` test verifies that reading sector 2 from the disk returns valid data by hardcoding expected superblock magic bytes:

```c
TEST(virtio_read_returns_data) {
	int ret = virtio_disk_read(2, read_buf);
	ASSERT_EQ(ret, 0, "Read should succeed");
	ASSERT_EQ(read_buf[0], 0x40, "Superblock magic byte 0");  // Line 100
	ASSERT_EQ(read_buf[1], 0x30, "Superblock magic byte 1");  // Line 101
	ASSERT_EQ(read_buf[2], 0x20, "Superblock magic byte 2");  // Line 102
	ASSERT_EQ(read_buf[3], 0x10, "Superblock magic byte 3");  // Line 103
	return 0;
}
```

These bytes (`0x40, 0x30, 0x20, 0x10`) represent the little-endian encoding of `FSMAGIC = 0x10203040`, defined in `/Users/davidklassen/work/davidklassen/slopix/kernel/fs.h` (line 13) and `/Users/davidklassen/work/davidklassen/slopix/cmd/mkfs/mkfs.c` (line 20).

Additionally, there's a duplicate hardcoded magic check in `virtio_intr_fires` test (line 182) that has the same problem.

## Why This Is a Problem

1. **Brittle Coupling to Disk Image**: The test directly couples to the physical disk image's contents rather than the filesystem specification. If the disk image is regenerated with different metadata, the test fails even if the virtio device driver works correctly.

2. **Test Becomes Disk Image Validator**: The test verifies the disk image contents, not the virtio read functionality. This conflates two concerns: "Can I read from disk?" and "Does this specific disk image contain these bytes?"

3. **No Constant Reuse**: The magic value is hardcoded as byte literals instead of referencing `FSMAGIC` or deriving it from the constant. If `FSMAGIC` changes in the future, the disk image builder (mkfs) will use the new value, but these tests won't be updated automatically.

4. **Hides Real Issues**: If reading returns garbage data, the test will fail, but it won't distinguish between:
   - Virtio read is broken (test should fail)
   - Disk image doesn't contain expected filesystem (disk builder issue, not virtio)
   - Disk image format changed (expected, tests should not break)

5. **Duplicated Magic Checks**: The `virtio_intr_fires` test (line 182) repeats the same hardcoded byte check, increasing fragility.

## When It Will Break

1. **If disk image is regenerated** with different contents or size
2. **If filesystem magic is changed** in mkfs or fs.h (values are not kept in sync with tests)
3. **If disk image builder changes** the superblock location or format
4. **If test environment uses a different disk image** for CI/local testing

## Test Recommendations

### 1. Decouple from Disk Image Contents

Instead of checking specific byte values, verify that:
- The read succeeds (already done on line 99)
- The returned data is valid according to the filesystem spec
- The superblock can be interpreted as a valid inode superblock

### 2. Use Filesystem Constants

Extract the magic constant from the kernel's filesystem header and use it:

```c
#include "fs.h"  // Provides FSMAGIC constant

TEST(virtio_read_returns_data) {
	int ret = virtio_disk_read(2, read_buf);
	ASSERT_EQ(ret, 0, "Read should succeed");

	// Read magic as little-endian 32-bit value
	unsigned int magic = read_buf[0] |
	                     (read_buf[1] << 8) |
	                     (read_buf[2] << 16) |
	                     (read_buf[3] << 24);

	ASSERT_EQ(magic, FSMAGIC, "Superblock magic should match FSMAGIC constant");
	return 0;
}
```

### 3. Make Tests Agnostic to Disk Image

Alternatively, remove the magic check entirely and test only the mechanics of reading:

```c
TEST(virtio_read_returns_data) {
	int ret = virtio_disk_read(2, read_buf);
	ASSERT_EQ(ret, 0, "Read from sector 2 should succeed");
	// Don't check contents - disk image format is not virtio's concern
	return 0;
}
```

The `virtio_intr_fires` test should be similarly decoupled, testing only that interrupt-driven reads work, not what data they return.

### 4. Separate Concerns with Integration Tests

If validating that a real filesystem image boots correctly is important:

```c
// This belongs in integration tests, not unit tests
TEST(filesytem_superblock_valid) {
	// Only run if a known-good filesystem image is mounted
	// This test validates the disk builder, not the virtio driver
	unsigned int magic = read_superblock_magic();
	ASSERT_EQ(magic, FSMAGIC, "Filesystem should have valid magic");
}
```

## Fixing Recommendations

### Recommended Fix: Use FSMAGIC Constant

1. **Update** `/Users/davidklassen/work/davidklassen/slopix/kernel/tests/test_virtio.c`:
   - Add `#include "fs.h"` (or relevant header with FSMAGIC)
   - Replace hardcoded magic bytes with reference to `FSMAGIC`
   - Fix both `virtio_read_returns_data` (line 97-105) and `virtio_intr_fires` (line 182)

2. **Example fix**:
   ```c
   #include "fs.h"  // Add this at top

   TEST(virtio_read_returns_data) {
       int ret = virtio_disk_read(2, read_buf);
       ASSERT_EQ(ret, 0, "Read should succeed");

       unsigned int magic = read_buf[0] |
                           (read_buf[1] << 8) |
                           (read_buf[2] << 16) |
                           (read_buf[3] << 24);
       ASSERT_EQ(magic, FSMAGIC, "Should read filesystem superblock magic");
       return 0;
   }
   ```

### Alternative Fix: Remove Magic Checks

If the intent is only to verify that virtio reads return *something*, remove the magic validation:

```c
TEST(virtio_read_returns_data) {
	int ret = virtio_disk_read(2, read_buf);
	ASSERT_EQ(ret, 0, "Read should succeed");
	// Data validation belongs to filesystem tests, not virtio tests
	return 0;
}
```

Remove the hardcoded byte check from `virtio_intr_fires` as well.

## Notes

According to `CLAUDE.md` test design guidelines:
> Tests are **read-only** - they check that init functions set correct state

The `virtio_read_returns_data` test violates this by verifying specific disk image contents rather than observing virtio device state. The test should either:

1. Use the `FSMAGIC` constant from the filesystem layer (preferred - maintains coupling to spec)
2. Remove the content validation entirely (cleanest - virtio shouldn't care about filesystem format)

The magic bytes issue compounds over time: every time someone regenerates the disk image, they must remember to verify these hardcoded test values match.
