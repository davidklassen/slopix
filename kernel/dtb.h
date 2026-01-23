#ifndef DTB_H
#define DTB_H

#define FDT_MAGIC 0xd00dfeed

void dtb_init(void *dtb_addr);
const char *dtb_get_bootargs(void);

#endif
