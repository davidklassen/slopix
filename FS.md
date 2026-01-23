# Filesystem Implementation Plan

Implementation plan for the Slopix filesystem.

**Parent**: [ROADMAP.md](ROADMAP.md) Milestone 11: Filesystem

## Overview

The filesystem provides file-based access to persistent storage. It builds on the block cache layer and exposes Unix-style file operations to userspace.

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
|   File Layer     |  struct file, file descriptor table
+------------------+
        |
+------------------+
|   Inode Layer    |  struct inode, path lookup
+------------------+
        |
+------------------+
|   Block Cache    |  bread(), bwrite(), brelse()
+------------------+
        |
+------------------+
|   Virtio Driver  |  virtio_disk_read/write()
+------------------+
```

## On-Disk Format

The filesystem uses xv6-style layout. See [DESIGN.md](DESIGN.md) "Filesystem Layout" for complete specification.

### Disk Layout

Block size: 1024 bytes (2 sectors)

```
Block 0:      Boot block (unused)
Block 1:      Superblock
Block 2-L:    Inode blocks
Block L+1:    Bitmap block (data block allocation)
Block B+1:    Data blocks
```

### Superblock

```c
struct superblock {
    uint32_t magic;       // 0x10203040
    uint32_t size;        // Total blocks in filesystem
    uint32_t nblocks;     // Number of data blocks
    uint32_t ninodes;     // Number of inodes
    uint32_t inodestart;  // First inode block
    uint32_t bmapstart;   // Bitmap block
};
```

### On-Disk Inode

64 bytes, 16 inodes per block.

```c
#define NDIRECT 12

struct dinode {
    uint16_t type;        // 0=free, 1=file, 2=dir, 3=device
    uint16_t major;       // Device major number (if type=device)
    uint16_t minor;       // Device minor number
    uint16_t nlink;       // Number of directory links
    uint32_t size;        // File size in bytes
    uint32_t addrs[NDIRECT+1];  // Data block addresses (last is indirect)
};
```

Max file size: 12 direct + 256 indirect = 268 blocks = 268KB

### Directory Entry

16 bytes, 64 entries per block.

```c
#define DIRSIZ 14

struct dirent {
    uint16_t inum;        // Inode number (0 = free entry)
    char name[DIRSIZ];    // File name (null-padded)
};
```

### In-Memory Inode

```c
struct inode {
    uint32_t dev;         // Device number
    uint32_t inum;        // Inode number
    int ref;              // Reference count
    int valid;            // Has been read from disk?
    int locked;           // Is inode locked?

    // Copy of on-disk inode
    uint16_t type;
    uint16_t major;
    uint16_t minor;
    uint16_t nlink;
    uint32_t size;
    uint32_t addrs[NDIRECT+1];
};
```

### Open File

```c
#define FD_NONE   0
#define FD_PIPE   1
#define FD_INODE  2
#define FD_DEVICE 3

struct file {
    int type;             // FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE
    int ref;              // Reference count
    int readable;
    int writable;
    struct inode *ip;     // FD_INODE, FD_DEVICE (NULL for console)
    uint32_t off;         // FD_INODE
    short major;          // FD_DEVICE: device major number
};
```

## Milestones

### F1: mkfs Host Tool ✓

Create filesystem images on the development host.

- [x] Create `tools/mkfs.c`:
  - Parse command-line arguments (image file, size)
  - Write superblock with correct offsets
  - Initialize inode blocks (all type=0)
  - Initialize bitmap (mark metadata blocks as used)
  - Create root directory (inode 1) with "." and ".." entries
- [x] Add to Makefile: `make mkfs`, `make disk.img`
- [x] Update test.img generation to use mkfs

**Exit criteria**: `make disk.img` creates a valid filesystem image.

### F2: Superblock and Inode Layer ✓

Read filesystem metadata from disk.

- [x] Define filesystem constants in `fs.h`:
  - BSIZE, NDIRECT, NINDIRECT, DIRSIZ
  - ROOTINO (1), T_DIR, T_FILE, T_DEVICE
- [x] Implement `readsb()`: read and cache superblock
- [x] Implement inode cache (fixed array of NINODE entries)
- [x] Implement `iget(dev, inum)`: get inode reference (no disk read)
- [x] Implement `ilock(ip)`: lock inode, read from disk if !valid
- [x] Implement `iunlock(ip)`: unlock inode
- [x] Implement `iput(ip)`: release inode reference
- [x] Implement `bmap(ip, bn)`: map logical block number to physical
- [x] Add `fs` test suite

**Exit criteria**: Can read superblock and inode 1 (root directory) from disk.

### F3: Directory Operations ✓

Navigate the directory tree.

- [x] Implement `readi(ip, dst, off, n)`: read n bytes from inode at offset
- [x] Implement `idup(ip)`: increment inode reference count
- [x] Implement `iunlockput(ip)`: combined unlock and put
- [x] Implement `dirlookup(dp, name, poff)`: find name in directory
- [x] Implement `skipelem(path, name)`: parse next path component
- [x] Implement `namex(path, nameiparent, name)`: resolve path to inode
- [x] Implement `namei(path)`: resolve path, return inode
- [x] Implement `nameiparent(path, name)`: resolve parent, copy final name
- [x] Add `cwd` (current working directory) to struct proc
- [x] Initialize first process with cwd = root inode
- [x] Copy cwd in fork(), release in exit()
- [x] Add string utilities (`strncmp`, `strncpy`, `memmove`, `memset`) in `kstring.c`
- [x] Add `fs_dir` test suite

**Exit criteria**: Can resolve "/", "/file", "/dir/file" paths (absolute and relative).

### F4: File Reading ✓

Read file contents.

- [x] Implement `readi(ip, dst, off, n)`: read n bytes from inode at offset (done in F3)
- [x] Implement `stati(ip, st)`: fill stat structure from inode
- [x] Add `fs_read` test suite

**Exit criteria**: Can read file contents from inode.

### F5: File Descriptor Layer ✓

Manage open files per process.

- [x] Define `struct file` and global file table (NFILE entries)
- [x] Implement `filealloc()`: allocate file structure
- [x] Implement `filedup(f)`: increment reference count
- [x] Implement `fileclose(f)`: decrement ref, cleanup if zero
- [x] Implement `filestat(f, st)`: get file stats
- [x] Implement `fileread(f, addr, n)`: read from file
- [x] Add `ofile[NOFILE]` array to struct proc
- [x] Add `cwd` (current working directory) to struct proc (done in F3)
- [x] Implement `fdalloc(f)`: allocate file descriptor slot
- [x] Initialize first process with cwd = root inode (done in F3)
- [x] Copy file descriptors in fork() (cwd done in F3)
- [x] Close file descriptors in exit() (cwd done in F3)

**Exit criteria**: Process can hold open file references.

### F6: Console Device ✓

Implement console as a device file.

- [x] Define device switch table: `struct devsw { read, write }`
- [x] Implement `consoleread()`: read from UART
- [x] Implement `consolewrite()`: write to UART
- [x] Register console as device (major=1)
- [x] Modify sys_read/sys_write to use file descriptor layer
- [x] Special case fd 0,1,2 as console device (before /dev/console exists)
- [x] Implement `filewrite(f, addr, n)`: write to file (FD_DEVICE only)
- [x] Add `console` test suite

**Exit criteria**: sys_read(0) and sys_write(1) work through file layer.

### F7: File Syscalls (Read-Only) ✓

Expose read-only file operations to userspace.

- [x] Implement `sys_open(path, flags)`:
  - Resolve path with namei()
  - Allocate file structure
  - Set readable/writable based on O_RDONLY, O_WRONLY, O_RDWR
  - Allocate file descriptor
  - Return fd
- [x] Implement `sys_close(fd)`: close file descriptor
- [x] Implement `sys_read(fd, buf, n)`: read through file layer (done in F6)
- [x] Implement `sys_write(fd, buf, n)`: write through file layer (done in F6, device only)
- [x] Implement `sys_fstat(fd, stat)`: get file info
- [x] Implement `sys_dup(fd)`: duplicate file descriptor
- [x] Add userspace `open()`, `close()`, `fstat()`, `dup()` wrappers
- [x] Add `filesys` userspace test suite

**Exit criteria**: User program can open and read files.

### F8: File Writing ✓

Extend to support writes.

- [x] Implement `writei(ip, src, off, n)`: write n bytes to inode
- [x] Implement `balloc(dev)`: allocate data block from bitmap
- [x] Implement `bfree(dev, b)`: free data block
- [x] Extend bmap() to allocate blocks on write
- [x] Implement `filewrite(f, addr, n)`: write to file
- [x] Implement `itrunc(ip)`: truncate file to zero (free all blocks)
- [x] Handle O_TRUNC in sys_open
- [x] Implement `iupdate(ip)`: write inode to disk
- [x] Add write tests to `filesys` userspace test suite

**Exit criteria**: User program can write to files.

### F9: File Creation and Deletion ✓

Create and remove files and directories.

- [x] Implement `ialloc(dev, type)`: allocate new inode
- [x] Implement `iupdate(ip)`: write inode to disk (done in F8)
- [x] Implement `dirlink(dp, name, inum)`: add directory entry
- [x] Implement `isdirempty(dp)`: check if directory is empty
- [x] Implement `create(path, type, major, minor)`: create file/dir/device
- [x] Handle O_CREAT in sys_open
- [x] Implement `sys_mkdir(path)`: create directory
- [x] Implement `sys_mknod(path, major, minor)`: create device node
- [x] Implement `sys_link(old, new)`: create hard link
- [x] Implement `sys_unlink(path)`: remove file or empty directory
- [x] Implement `sys_chdir(path)`: change current directory
- [x] Modify `iput()` to free inode when nlink reaches 0
- [x] Add userspace wrappers (`mkdir`, `mknod`, `link`, `unlink`, `chdir`)
- [x] Add file creation/deletion tests to `filesys` test suite

**Exit criteria**: User program can create, link, and delete files.

### F10: Pipes ✓

Inter-process communication via pipes.

- [x] Define `struct pipe` with buffer and read/write offsets
- [x] Implement `pipealloc(fd0, fd1)`: create pipe pair
- [x] Implement `piperead(pi, addr, n)`: read from pipe
- [x] Implement `pipewrite(pi, addr, n)`: write to pipe
- [x] Implement `pipeclose(pi, writable)`: close pipe end
- [x] Implement `sys_pipe(fdarray)`: create pipe, return fds
- [x] Add pipe support to fileread/filewrite
- [x] Add `pipe` test suite

**Exit criteria**: Two processes can communicate via pipe.

## Device Nodes

The filesystem supports device files through the mknod syscall.

| Major | Minor | Device |
|-------|-------|--------|
| 1     | 0     | Console (/dev/console) |

Future devices can be added by registering in the devsw table.

## Test Strategy

### Kernel Tests

Tests run before scheduler starts (single-threaded).

```c
TEST_SUITE(fs) {
    RUN_TEST(fs_superblock_valid);      // magic number correct
    RUN_TEST(fs_superblock_sizes);      // sizes non-zero
    RUN_TEST(fs_iget_root);             // can get root inode
    RUN_TEST(fs_ilock_root);            // can lock and read root
}

TEST_SUITE(fs_dir) {
    RUN_TEST(fs_dir_dot);               // "." in root
    RUN_TEST(fs_dir_dotdot);            // ".." in root
    RUN_TEST(fs_namei_root);            // namei("/") returns root
    RUN_TEST(fs_namei_file);            // namei("/file") works
    RUN_TEST(fs_namei_relative);        // namei("file") with cwd
    RUN_TEST(fs_namei_relative_dot);    // namei(".") with cwd
}

TEST_SUITE(fs_read) {
    RUN_TEST(fs_readi_small);           // read small file
    RUN_TEST(fs_readi_offset);          // read from offset
    RUN_TEST(fs_readi_eof);             // read clamped to EOF
    RUN_TEST(fs_stati);                 // stat structure from inode
    RUN_TEST(fs_readi_large);           // read multi-block file
}

TEST_SUITE(fs_file) {
    RUN_TEST(file_alloc_basic);         // filealloc returns ref=1
    RUN_TEST(file_dup_increments_ref);  // filedup increases ref
    RUN_TEST(file_close_decrements_ref);// fileclose decreases ref
    RUN_TEST(file_close_releases_inode);// ref=0 releases inode
    RUN_TEST(file_stat_from_inode);     // filestat copies inode stats
    RUN_TEST(file_read_advances_offset);// fileread updates offset
    RUN_TEST(file_read_not_readable);   // fileread fails if !readable
    RUN_TEST(file_fdalloc_lowest);      // fdalloc returns lowest fd
}

TEST_SUITE(console) {
    RUN_TEST(console_devsw_registered);     // devsw[CONSOLE] has read/write
    RUN_TEST(console_write_basic);          // consolewrite returns count
    RUN_TEST(console_file_device_type);     // file can be FD_DEVICE
    RUN_TEST(console_filewrite);            // filewrite to console works
    RUN_TEST(console_filewrite_not_writable); // filewrite fails if !writable
}
```

### Userspace Tests

Tests run as user processes (multi-threaded). Test suites are split into separate files in `cmd/tests/`.

```c
TEST_SUITE(filesys) {
    RUN_TEST(open_file);                // open and close file
    RUN_TEST(open_nonexistent);         // open nonexistent returns -1
    RUN_TEST(open_read_file);           // open and read contents
    RUN_TEST(fstat_file);               // fstat returns file info
    RUN_TEST(dup_file);                 // dup returns new fd
    RUN_TEST(close_invalid_fd);         // close invalid fd returns -1
    RUN_TEST(read_after_close);         // read closed fd returns -1
    RUN_TEST(write_file);               // write to file
    RUN_TEST(write_read_back);          // write then read back
    RUN_TEST(open_trunc);               // O_TRUNC truncates file
    RUN_TEST(write_extends_file);       // write extends file size
    RUN_TEST(create_file);              // O_CREAT creates file
    RUN_TEST(mkdir_basic);              // mkdir creates directory
    RUN_TEST(link_unlink);              // link and unlink files
    RUN_TEST(chdir_basic);              // chdir changes cwd
    RUN_TEST(unlink_nonexistent);       // unlink nonexistent fails
    RUN_TEST(mkdir_duplicate);          // mkdir duplicate fails
}

// Future test suites:
// void test_pipe(void);                // pipe communication
```

### Test Filesystem

The mkfs tool supports adding files during image creation:

```bash
./mkfs disk.img testfile.txt:/test.txt
```

This adds `testfile.txt` from the host as `/test.txt` in the image.

The test disk image includes `/hello` (from `testdata/hello.txt`) for testing path resolution and `/large` (from `testdata/large.txt`, 12KB) for testing multi-block reads.

## Implementation Notes

### Concurrency

The block cache serializes disk access via `disk_wait()`/`disk_done()`. The inode cache uses reference counting for safe concurrent access. Inode locking (via `ilock`/`iunlock`) protects inode contents during read/write.

### Error Handling

Functions return -1 on error. Syscalls that fail leave errno-style error codes (future enhancement). Disk errors from the virtio driver propagate up through the layers.

### Memory Layout

- Inode cache: NINODE (50) entries, statically allocated
- File table: NFILE (100) entries, statically allocated
- Per-process: NOFILE (16) file descriptor slots

## References

- [DESIGN.md](DESIGN.md): On-disk format, virtio driver details
- [xv6 Book Chapter 8](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf): File system design
- [xv6-riscv fs.c](https://github.com/mit-pdos/xv6-riscv/blob/riscv/kernel/fs.c): Reference implementation
