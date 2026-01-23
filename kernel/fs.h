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

#endif
