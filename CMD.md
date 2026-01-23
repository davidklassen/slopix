# Userspace Commands

Tracking userspace programs and tools needed for Slopix.

## Host Tools

Tools that run on the development host (Linux/macOS).

### mkfs

Creates a Slopix filesystem image.

**Status**: Not started

**Usage**:
```bash
./mkfs disk.img
```

**Features**:
- Initialize superblock with filesystem metadata
- Create root directory with "." and ".." entries
- Allocate inode and data block bitmaps
- Support specifying filesystem size

## Userspace Programs

Programs that run inside Slopix.

### shell

Interactive command interpreter. Currently exists as minimal shell.

**Status**: Basic implementation exists

**Planned enhancements** (for shell milestone):
- Command parsing with arguments
- I/O redirection (>, <, >>)
- Pipes (|)
- Built-in commands: cd, exit, pwd

### ls

List directory contents.

**Status**: Not started

**Features**:
- List files in current directory
- Show file sizes and types
- Support path argument

### cat

Concatenate and display file contents.

**Status**: Not started

**Features**:
- Read file and print to stdout
- Support multiple file arguments

### echo

Print arguments to stdout.

**Status**: Not started

### mkdir

Create directories.

**Status**: Not started

### rm

Remove files.

**Status**: Not started

### cp

Copy files.

**Status**: Not started

### mv

Move/rename files.

**Status**: Not started

### touch

Create empty file or update timestamp.

**Status**: Not started

### wc

Word/line/character count.

**Status**: Not started

## Implementation Order

1. **mkfs** (host) - Required to create test filesystem images
2. **ls** - Essential for filesystem debugging
3. **cat** - Essential for reading files
4. **echo** - Useful for testing and scripts
5. **mkdir**, **rm** - Directory manipulation
6. **cp**, **mv** - File manipulation
7. **wc**, **touch** - Utilities

## References

- [xv6-riscv user programs](https://github.com/mit-pdos/xv6-riscv/tree/riscv/user)
- [POSIX utilities](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/contents.html)
