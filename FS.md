# Filesystem Implementation Plan

Implementation plan for the Slopix filesystem layer.

**Parent**: [ROADMAP.md](ROADMAP.md) Milestone 11: Filesystem

## Overview

The filesystem provides file-based access to persistent storage. It sits above the block cache layer (VIRTIO.md M7) and exposes file syscalls to userspace.

```
+------------------+
|   User Program   |  open(), read(), write(), close()
+------------------+
        | syscall
+------------------+
|   File Syscalls  |  sys_open, sys_read, sys_write, sys_close
+------------------+
        |
+------------------+
|   VFS Layer      |  inode operations, path lookup
+------------------+
        |
+------------------+
|   Block Cache    |  bread(), bwrite(), brelse()  [VIRTIO.md M7]
+------------------+
        |
+------------------+
|   Virtio Driver  |  virtio_disk_read/write()    [VIRTIO.md M1-M6]
+------------------+
```

## Critical Dependencies

### Virtio Driver Concurrency Limitation

**IMPORTANT**: The current virtio driver (as of M6) does NOT support concurrent disk requests.

The driver uses global variables for request state:
```c
static struct virtio_blk_outhdr blk_hdr;  // single shared header
static unsigned char blk_status;           // single shared status
```

If process A calls `virtio_disk_read()` and sleeps, then process B calls `virtio_disk_read()` before A completes, B will overwrite `blk_hdr`, corrupting A's in-flight request.

**Resolution options** (must choose one before filesystem implementation):

1. **Serialize at block cache layer**: Add a global lock in bread()/bwrite() so only one disk request is in flight at a time. Simple but limits I/O parallelism.

2. **Fix virtio driver**: Allocate `blk_hdr` and `blk_status` per-request (on stack or dynamically). Requires tracking which buffer corresponds to which descriptor. More complex but enables concurrent I/O.

3. **Serialize at virtio layer**: Add a lock inside `virtio_disk_rw()`. Same effect as option 1 but at a lower level.

**Recommendation**: Start with option 1 (serialize at block cache) for simplicity. This is what xv6 does. Concurrent I/O optimization can come later.

### Block Cache (VIRTIO.md M7)

The filesystem depends on the block cache layer defined in VIRTIO.md M7:
- `bread(dev, blockno)` - read block, return locked buffer
- `bwrite(buf)` - write buffer to disk
- `brelse(buf)` - release buffer

The block cache must be implemented before the filesystem.

## On-Disk Format

See [DESIGN.md](DESIGN.md) "Filesystem Layout" section for complete specification.

Summary:
- Block size: 1024 bytes
- xv6-style layout: superblock, log, inodes, bitmap, data blocks
- Inode: 12 direct blocks + 1 indirect block (max 268KB per file)
- Directory entries: 14-char name + 2-byte inode number

## Implementation Milestones

### F1: Block Cache (VIRTIO.md M7)

Prerequisite milestone - implement bread/bwrite/brelse with concurrency protection.

See VIRTIO.md M7 for details.

### F2: Superblock and Inode Reading

Read-only access to filesystem metadata.

- [ ] Implement `readsb()`: read and cache superblock
- [ ] Implement `iget(inum)`: get inode reference
- [ ] Implement `ilock(ip)`: lock inode and read from disk if needed
- [ ] Implement `iunlock(ip)`: unlock inode
- [ ] Implement `iput(ip)`: release inode reference
- [ ] Implement `bmap(ip, bn)`: map logical block to physical block
- [ ] Add `fs_super` test suite (2 tests)

**Exit criteria**: Can read superblock and inode structures from disk.

### F3: File Reading

Read file contents.

- [ ] Implement `readi(ip, dst, off, n)`: read data from inode
- [ ] Implement `stati(ip, st)`: get file stat info
- [ ] Add `fs_read` test suite (3 tests)

**Exit criteria**: Can read file contents given an inode.

### F4: Directory Operations

Navigate the directory tree.

- [ ] Implement `dirlookup(dp, name)`: find entry in directory
- [ ] Implement `namei(path)`: resolve path to inode
- [ ] Implement `nameiparent(path, name)`: resolve parent directory
- [ ] Add `fs_dir` test suite (3 tests)

**Exit criteria**: Can resolve "/path/to/file" to an inode.

### F5: File Syscalls (Read-Only)

Expose read-only file operations to userspace.

- [ ] Implement file descriptor table (per-process)
- [ ] Implement `sys_open(path, flags)`: open file, return fd
- [ ] Implement `sys_close(fd)`: close file descriptor
- [ ] Implement `sys_read(fd, buf, n)`: read from file
- [ ] Implement `sys_fstat(fd, stat)`: get file info
- [ ] Add `syscall_file` test suite (4 tests)

**Exit criteria**: User program can open and read files.

### F6: File Writing

Extend to support writes.

- [ ] Implement `writei(ip, src, off, n)`: write data to inode
- [ ] Implement `itrunc(ip)`: truncate file to zero length
- [ ] Implement `sys_write(fd, buf, n)`: write to file (for regular files)
- [ ] Add `fs_write` test suite (3 tests)

**Exit criteria**: User program can write to files.

### F7: File Creation and Deletion

Create and remove files.

- [ ] Implement `ialloc(type)`: allocate new inode
- [ ] Implement `dirlink(dp, name, inum)`: add directory entry
- [ ] Implement `sys_open` with O_CREAT flag
- [ ] Implement `sys_unlink(path)`: remove file
- [ ] Implement `sys_mkdir(path)`: create directory
- [ ] Add `fs_create` test suite (4 tests)

**Exit criteria**: User program can create and delete files.

### F8: Logging (Optional)

Crash recovery via write-ahead logging.

- [ ] Implement `begin_op()`: start filesystem operation
- [ ] Implement `end_op()`: commit filesystem operation
- [ ] Implement `log_write(buf)`: mark buffer for logging
- [ ] Wrap all modifying operations in begin_op/end_op
- [ ] Add `fs_log` test suite (2 tests)

**Exit criteria**: Filesystem recovers consistently after simulated crash.

## Testing Strategy

### Test Disk Image

Create a test disk image with known filesystem contents:
```bash
# Create empty disk
dd if=/dev/zero of=test.img bs=1M count=1

# Format with mkfs tool (to be written)
./mkfs test.img
```

### Kernel Tests

Tests run before scheduler starts, so they test synchronous single-threaded access.

```c
TEST_SUITE(fs_super) {
    RUN_TEST(fs_superblock_valid);      // magic number correct
    RUN_TEST(fs_superblock_sizes);      // sizes non-zero
}

TEST_SUITE(fs_read) {
    RUN_TEST(fs_read_root_inode);       // can read inode 1
    RUN_TEST(fs_read_file_contents);    // can read known file
    RUN_TEST(fs_read_large_file);       // can read multi-block file
}

TEST_SUITE(fs_dir) {
    RUN_TEST(fs_dir_lookup);            // find entry in directory
    RUN_TEST(fs_namei_simple);          // resolve "/file"
    RUN_TEST(fs_namei_nested);          // resolve "/dir/file"
}
```

### Userspace Tests

Tests run as user processes after scheduler starts. These can test concurrent file access.

```c
TEST_SUITE(file_concurrent) {
    RUN_TEST(file_two_readers);         // two processes read same file
    RUN_TEST(file_reader_writer);       // one reads, one writes different file
}
```

## References

- [DESIGN.md](DESIGN.md): On-disk format specification
- [VIRTIO.md](VIRTIO.md): Block device driver and cache
- [xv6 Book Chapter 8](docs/xv6-book-riscv/xv6-book-riscv.md): File system design
- [OSDev FAT](https://wiki.osdev.org/FAT): Alternative simple filesystem
