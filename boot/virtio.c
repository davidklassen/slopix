#define VIRTIO0_PA 0x0A003E00UL

#define VIRTIO_REG(off) (*(volatile unsigned int *)(VIRTIO0_PA + (off)))

#define VIRTIO_MMIO_MAGIC	    0x000
#define VIRTIO_MMIO_VERSION	    0x004
#define VIRTIO_MMIO_DEVICE_ID	    0x008
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL	    0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM	    0x038
#define VIRTIO_MMIO_QUEUE_ALIGN	    0x03c
#define VIRTIO_MMIO_QUEUE_PFN	    0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_STATUS	    0x070

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER	  2
#define VIRTIO_STATUS_DRIVER_OK	  4

#define VIRTIO_MAGIC_VALUE     0x74726976
#define VIRTIO_VERSION_LEGACY  1
#define VIRTIO_DEVICE_ID_BLOCK 2

#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_S_OK 0

#define PAGE_SIZE 4096UL

// Static memory layout below stack (stack is at 0x40080000):
// 0x40070000: desc[0..7] (8 descriptors * 16 bytes = 128 bytes)
// 0x40070080: avail ring (4 + 2*8 = 20 bytes)
// 0x40071000: used ring (at page boundary)
// 0x40072000: blk_hdr (16 bytes)
// 0x40072010: blk_status (1 byte)
#define VIRTQ_PA      0x40070000UL
#define DESC_PA	      VIRTQ_PA
#define AVAIL_PA      (VIRTQ_PA + 128)
#define USED_PA	      (VIRTQ_PA + PAGE_SIZE)
#define BLK_HDR_PA    (VIRTQ_PA + 2 * PAGE_SIZE)
#define BLK_STATUS_PA (BLK_HDR_PA + 16)

struct virtq_desc {
	unsigned long addr;
	unsigned int len;
	unsigned short flags;
	unsigned short next;
};

struct virtq_avail {
	unsigned short flags;
	unsigned short idx;
	unsigned short ring[8];
};

struct virtq_used_elem {
	unsigned int id;
	unsigned int len;
};

struct virtq_used {
	unsigned short flags;
	unsigned short idx;
	struct virtq_used_elem ring[8];
};

struct virtio_blk_outhdr {
	unsigned int type;
	unsigned int reserved;
	unsigned long sector;
};

static struct virtq_desc *desc = (struct virtq_desc *)DESC_PA;
static struct virtq_avail *avail = (struct virtq_avail *)AVAIL_PA;
static struct virtq_used *used = (struct virtq_used *)USED_PA;
static struct virtio_blk_outhdr *blk_hdr = (struct virtio_blk_outhdr *)BLK_HDR_PA;
static unsigned char *blk_status = (unsigned char *)BLK_STATUS_PA;

void uart_puts(const char *s);
void dsb(void);
void nop(void);

static void zero_mem(void *ptr, unsigned long size) {
	unsigned char *p = ptr;
	for (unsigned long i = 0; i < size; i++) {
		p[i] = 0;
	}
}

void virtio_init(void) {
	if (VIRTIO_REG(VIRTIO_MMIO_MAGIC) != VIRTIO_MAGIC_VALUE) {
		uart_puts("virtio: bad magic\n");
		return;
	}
	if (VIRTIO_REG(VIRTIO_MMIO_VERSION) != VIRTIO_VERSION_LEGACY) {
		uart_puts("virtio: bad version\n");
		return;
	}
	if (VIRTIO_REG(VIRTIO_MMIO_DEVICE_ID) != VIRTIO_DEVICE_ID_BLOCK) {
		uart_puts("virtio: not a block device\n");
		return;
	}

	VIRTIO_REG(VIRTIO_MMIO_STATUS) = 0;

	VIRTIO_REG(VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_ACKNOWLEDGE;
	VIRTIO_REG(VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

	(void)VIRTIO_REG(VIRTIO_MMIO_DEVICE_FEATURES);
	VIRTIO_REG(VIRTIO_MMIO_DRIVER_FEATURES) = 0;

	VIRTIO_REG(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PAGE_SIZE;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_SEL) = 0;

	unsigned int num_max = VIRTIO_REG(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (num_max == 0) {
		uart_puts("virtio: queue not available\n");
		return;
	}

	zero_mem((void *)VIRTQ_PA, 3 * PAGE_SIZE);

	VIRTIO_REG(VIRTIO_MMIO_QUEUE_NUM) = 8;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_ALIGN) = PAGE_SIZE;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_PFN) = VIRTQ_PA / PAGE_SIZE;

	dsb();

	VIRTIO_REG(VIRTIO_MMIO_STATUS) =
	    VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK;

	uart_puts("virtio: found block device\n");
}

int virtio_read(unsigned long sector, void *buf) {
	blk_hdr->type = VIRTIO_BLK_T_IN;
	blk_hdr->reserved = 0;
	blk_hdr->sector = sector;

	// Descriptor 0: blk_hdr (device reads)
	desc[0].addr = BLK_HDR_PA;
	desc[0].len = 16;
	desc[0].flags = VIRTQ_DESC_F_NEXT;
	desc[0].next = 1;

	// Descriptor 1: data buffer (device writes)
	desc[1].addr = (unsigned long)buf;
	desc[1].len = 512;
	desc[1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
	desc[1].next = 2;

	// Descriptor 2: status (device writes)
	desc[2].addr = BLK_STATUS_PA;
	desc[2].len = 1;
	desc[2].flags = VIRTQ_DESC_F_WRITE;
	desc[2].next = 0;

	*blk_status = 0xFF;

	avail->ring[avail->idx % 8] = 0;
	dsb();
	avail->idx++;
	dsb();

	unsigned short last_used = used->idx;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

	unsigned long timeout = 100000000;
	while (used->idx == last_used) {
		if (--timeout == 0) {
			uart_puts("virtio: read timeout\n");
			return -1;
		}
		nop();
	}

	if (*blk_status != VIRTIO_BLK_S_OK) {
		uart_puts("virtio: read error\n");
		return -1;
	}

	return 0;
}
