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

#define T_FREE	 0
#define T_FILE	 1
#define T_DIR	 2
#define T_DEVICE 3

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR	 0x002

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

void fsinit(unsigned int dev);
void readsb(unsigned int dev, struct superblock *sb);
struct inode *iget(unsigned int dev, unsigned int inum);
struct inode *idup(struct inode *ip);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iput(struct inode *ip);
void iunlockput(struct inode *ip);
unsigned int bmap(struct inode *ip, unsigned int bn);
int readi(struct inode *ip, char *dst, unsigned int off, unsigned int n);
void stati(struct inode *ip, struct stat *st);
struct inode *dirlookup(struct inode *dp, char *name, unsigned int *poff);
struct inode *namei(char *path);
struct inode *nameiparent(char *path, char *name);

#endif
