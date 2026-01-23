#ifndef FS_H
#define FS_H

#define ROOTINO 1
#define BSIZE	1024

#define NDIRECT	  12
#define NINDIRECT (BSIZE / sizeof(unsigned int))
#define MAXFILE	  (NDIRECT + NINDIRECT)

#define DIRSIZ	14
#define FSMAGIC 0x10203040

#define IPB (BSIZE / sizeof(struct dinode))
#define BPB (BSIZE * 8)

#define IBLOCK(i, sb) ((i) / IPB + (sb).inodestart)
#define BBLOCK(b, sb) ((b) / BPB + (sb).bmapstart)

#define T_FREE	  0
#define T_FILE	  1
#define T_DIR	  2
#define T_DEVICE  3
#define T_BDEVICE 4

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR	 0x002
#define O_CREAT	 0x100
#define O_TRUNC	 0x200
#define O_APPEND 0x400

struct superblock {
	unsigned int magic;
	unsigned int size;
	unsigned int nblocks;
	unsigned int ninodes;
	unsigned int inodestart;
	unsigned int bmapstart;
};

struct dinode {
	unsigned short type;
	unsigned short major;
	unsigned short minor;
	unsigned short nlink;
	unsigned int size;
	unsigned int addrs[NDIRECT + 1];
};

struct dirent {
	unsigned short inum;
	char name[DIRSIZ];
};

#define NINODE 50

struct inode {
	unsigned int dev;
	unsigned int inum;
	int ref;
	int valid;
	int locked;

	unsigned short type;
	unsigned short major;
	unsigned short minor;
	unsigned short nlink;
	unsigned int size;
	unsigned int addrs[NDIRECT + 1];
};

struct stat {
	unsigned int dev;
	unsigned int ino;
	unsigned short type;
	unsigned short nlink;
	unsigned int size;
};

void fs_init(unsigned int dev);
void fs_readsb(unsigned int dev, struct superblock *sb);
struct inode *fs_iget(unsigned int dev, unsigned int inum);
struct inode *fs_idup(struct inode *ip);
void fs_ilock(struct inode *ip);
void fs_iunlock(struct inode *ip);
void fs_iput(struct inode *ip);
void fs_iupdate(struct inode *ip);
void fs_iunlockput(struct inode *ip);
unsigned int fs_bmap(struct inode *ip, unsigned int bn);
int fs_readi(struct inode *ip, char *dst, unsigned int off, unsigned int n);
int fs_writei(struct inode *ip, const char *src, unsigned int off, unsigned int n);
void fs_itrunc(struct inode *ip);
void fs_stati(struct inode *ip, struct stat *st);
struct inode *fs_ialloc(unsigned int dev, unsigned short type);
int fs_dirlink(struct inode *dp, char *name, unsigned int inum);
int fs_isdirempty(struct inode *dp);
struct inode *fs_create(char *path, unsigned short type, unsigned short major, unsigned short minor);
struct inode *fs_dirlookup(struct inode *dp, char *name, unsigned int *poff);
struct inode *fs_namei(char *path);
struct inode *fs_nameiparent(char *path, char *name);

#endif
