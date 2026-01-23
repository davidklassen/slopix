#ifndef VIRTIO_H
#define VIRTIO_H

#include "board.h"

#define VIRTIO_REG(off) (*(volatile unsigned int *)(VIRTIO0_VA + (off)))

// MMIO register offsets
#define VIRTIO_MMIO_MAGIC		0x000
#define VIRTIO_MMIO_VERSION		0x004
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_VENDOR_ID		0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028
#define VIRTIO_MMIO_QUEUE_SEL		0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define VIRTIO_MMIO_QUEUE_NUM		0x038
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c
#define VIRTIO_MMIO_QUEUE_PFN		0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064
#define VIRTIO_MMIO_STATUS		0x070
#define VIRTIO_MMIO_CONFIG		0x100

// Status bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER	  2
#define VIRTIO_STATUS_DRIVER_OK	  4

// Expected values
#define VIRTIO_MAGIC_VALUE     0x74726976
#define VIRTIO_VERSION_LEGACY  1
#define VIRTIO_DEVICE_ID_BLOCK 2
#define VIRTIO_VENDOR_ID_QEMU  0x554d4551

// Queue configuration
#define VIRTIO_QUEUE_SIZE 8

// Descriptor flags
#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

// Block request types
#define VIRTIO_BLK_T_IN	 0
#define VIRTIO_BLK_T_OUT 1

// Block status values
#define VIRTIO_BLK_S_OK	    0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

// VIRTIO0 interrupt (SPI 16 = INTID 48)
#define VIRTIO_IRQ 48

// Block request header
struct virtio_blk_outhdr {
	unsigned int type;
	unsigned int reserved;
	unsigned long sector;
};

struct virtq_desc {
	unsigned long addr;
	unsigned int len;
	unsigned short flags;
	unsigned short next;
};

struct virtq_avail {
	unsigned short flags;
	unsigned short idx;
	unsigned short ring[VIRTIO_QUEUE_SIZE];
};

struct virtq_used_elem {
	unsigned int id;
	unsigned int len;
};

struct virtq_used {
	unsigned short flags;
	unsigned short idx;
	struct virtq_used_elem ring[VIRTIO_QUEUE_SIZE];
};

// API
void virtio_init(void);
void virtio_init_irq(void);
void virtio_intr(void);
int virtio_probe(void);
void virtio_reset(void);
int virtio_disk_read(unsigned long sector, void *buf);
int virtio_disk_write(unsigned long sector, void *buf);

#endif
