#ifndef BIO_H
#define BIO_H

#include "buf.h"

void binit(void);
struct buf *bread(unsigned int dev, unsigned int blockno);
void bwrite(struct buf *b);
void brelse(struct buf *b);

#endif
