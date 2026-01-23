#ifndef BIO_H
#define BIO_H

#include "buf.h"

void bio_init(void);
struct buf *bio_read(unsigned int dev, unsigned int blockno);
int bio_write(struct buf *b);
void bio_release(struct buf *b);

#endif
