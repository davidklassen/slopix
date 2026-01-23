#ifndef BIO_H
#define BIO_H

#include "buf.h"

void bio_init(void);
struct buf *bread(unsigned int dev, unsigned int blockno);
int bwrite(struct buf *b);
void brelse(struct buf *b);

#endif
