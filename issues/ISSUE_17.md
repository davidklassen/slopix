# Return Value Ambiguity in `bmap()` - Error vs. Boot Block

## Severity
**Critical**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/boot/fs.c`
- **Lines:** 88-114 (bmap function); error checks at lines 159 and 228

## Description

The `bmap()` function returns 0 on block I/O errors (lines 95, 104, 111), but 0 is also a valid block number representing the boot block (documented in DESIGN.md section "Filesystem Layout on-disk" as "Block 0: Boot block (unused)").

Callers cannot distinguish between:
1. An error reading from disk (read_block() failed)
2. A legitimate mapping to block 0 (the boot block)

This creates a silent data corruption risk: if an inode legitimately references block 0, the code will treat it as an error and fail to read the file, losing data. Conversely, actual I/O errors during block lookups are silently treated as if the block address is 0.

### Current Code

**bmap() function (lines 86-115):**
```c
static unsigned int bmap(struct dinode *dip, unsigned int bn) {
	if (bn < NDIRECT) {
		return dip->addrs[bn];
	}

	bn -= NDIRECT;

	if (bn < NINDIRECT) {
		if (read_block(dip->addrs[NDIRECT], indirect_buf) < 0)
			return 0;  // ERROR: returns 0 on failure
		unsigned int *addrs = (unsigned int *)indirect_buf;
		return addrs[bn];
	}

	bn -= NINDIRECT;

	unsigned int dindirect_block = dip->addrs[NDIRECT + 1];
	if (read_block(dindirect_block, indirect_buf) < 0)
		return 0;  // ERROR: returns 0 on failure

	unsigned int *l1 = (unsigned int *)indirect_buf;
	unsigned int l1_idx = bn / NINDIRECT;
	unsigned int l2_idx = bn % NINDIRECT;

	if (read_block(l1[l1_idx], indirect_buf) < 0)
		return 0;  // ERROR: returns 0 on failure

	unsigned int *l2 = (unsigned int *)indirect_buf;
	return l2[l2_idx];
}
```

**Callers checking for errors (lines 159, 228):**
```c
// In dirlookup() at line 159:
unsigned int blockno = bmap(dir, i);
if (blockno == 0)
	continue;

// In fs_read_file() at line 228:
unsigned int blockno = bmap(&inode, bn);
if (blockno == 0)
	return -1;
```

### Problem

1. **Error return ambiguity**: When `bmap()` returns 0 due to `read_block()` failure, the caller cannot know if the failure is real or if the inode legitimately maps to block 0.

2. **Data loss risk**: If a correctly-formatted filesystem has an inode with a block address of 0 (pointing to the boot block for some reason), the code will silently fail to read it.

3. **Inconsistent error handling**: The boot fs does not follow the kernel's error-return convention (documented in DESIGN.md line 98: "Integers: 0 on success, -1 on failure"). This inconsistency makes the code harder to audit and maintain.

4. **Silent degradation**: I/O errors in indirect block lookups are silently swallowed, losing diagnostic information about disk failures.

## How to Reproduce

### Scenario 1: Inode references block 0 (data loss)
1. Create a filesystem image with an inode that has a direct block pointer set to 0 (e.g., by manually crafting the disk image or modifying mkfs)
2. Load a file that uses this inode during boot
3. Expected: File contents from block 0 are read
4. Actual: `dirlookup()` skips the block (line 160 `continue`), or `fs_read_file()` returns -1 (line 229), losing access to the data

### Scenario 2: Disk read failure in indirect block lookup (silent I/O error)
1. Set up a filesystem with a large file requiring indirect blocks (>11 blocks)
2. Inject a read error in virtio_read() for the indirect block address
3. The bootloader will silently treat the error as "block 0" and attempt to load kernel from block 0 instead
4. Expected: Diagnostic error message or graceful failure
5. Actual: Silent failure, kernel loads corrupted or uninitialized data

## Test Recommendations

1. **Unit test for legitimate block 0 access**: If block 0 should never be referenced by inodes (as the design suggests it's "unused"), add an assertion in `bmap()` that rejects any addrs[] entry containing 0:
   ```c
   static unsigned int bmap(struct dinode *dip, unsigned int bn) {
       if (bn < NDIRECT) {
           unsigned int addr = dip->addrs[bn];
           assert(addr != 0 && "inode references block 0 (boot block)");
           return addr;
       }
       // ... rest of function
   }
   ```

2. **Integration test for disk I/O errors**: Mock `virtio_read()` to return -1 for specific block numbers, and verify that `fs_read_file()` fails gracefully rather than silently reading block 0.

3. **Filesystem integrity check**: During `fs_init()`, scan all inodes and verify that no addrs[] entries are 0 (except for entries that are legitimately empty in sparse files).

## Fixing Recommendation

Change `bmap()` to return signed integers and use -1 to indicate errors, matching the kernel's convention:

### Option 1: Use signed return type (recommended)
```c
static int bmap(struct dinode *dip, unsigned int bn) {
	if (bn < NDIRECT) {
		return dip->addrs[bn];
	}

	bn -= NDIRECT;

	if (bn < NINDIRECT) {
		if (read_block(dip->addrs[NDIRECT], indirect_buf) < 0)
			return -1;  // Error: -1
		unsigned int *addrs = (unsigned int *)indirect_buf;
		return addrs[bn];
	}

	bn -= NINDIRECT;

	unsigned int dindirect_block = dip->addrs[NDIRECT + 1];
	if (read_block(dindirect_block, indirect_buf) < 0)
		return -1;  // Error: -1

	unsigned int *l1 = (unsigned int *)indirect_buf;
	unsigned int l1_idx = bn / NINDIRECT;
	unsigned int l2_idx = bn % NINDIRECT;

	if (read_block(l1[l1_idx], indirect_buf) < 0)
		return -1;  // Error: -1

	unsigned int *l2 = (unsigned int *)indirect_buf;
	return l2[l2_idx];
}
```

Then update callers:
```c
// In dirlookup():
int blockno = bmap(dir, i);
if (blockno < 0)  // Check for error, not zero
	return 0;

// In fs_read_file():
int blockno = bmap(&inode, bn);
if (blockno < 0)  // Check for error, not zero
	return -1;
```

### Option 2: Add an output parameter for errors
```c
static unsigned int bmap(struct dinode *dip, unsigned int bn, int *err) {
	if (bn < NDIRECT) {
		*err = 0;
		return dip->addrs[bn];
	}

	// ... checks ...
	if (read_block(...) < 0) {
		*err = -1;
		return 0;  // Dummy value
	}
	*err = 0;
	return addr;
}
```

Option 1 is simpler and more aligned with the codebase convention. Ensure that block number 0 can actually be returned legitimately (or update the design to forbid it).
