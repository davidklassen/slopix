#include "virtio.h"
#include "cpu.h"
#include "kprintf.h"
#include "pmm.h"
#include "gic.h"
#include "proc.h"
#include "timer.h"

static struct virtq_desc *desc;
static struct virtq_avail *avail;
static struct virtq_used *used;
static unsigned short free_head;
static unsigned short last_used_idx;
static char virtio_disk_chan;

static struct virtio_blk_outhdr blk_hdr;
static unsigned char blk_status;

static int alloc_desc(void) {
	if (free_head == 0xFFFF) {
		return -1;
	}
	int idx = free_head;
	free_head = desc[idx].next;
	return idx;
}

static void free_desc(int i) {
	desc[i].next = free_head;
	free_head = i;
}

static int virtio_queue_init(void) {
	VIRTIO_REG(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PAGE_SIZE;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_SEL) = 0;

	unsigned int num_max = VIRTIO_REG(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (num_max == 0 || num_max < VIRTIO_QUEUE_SIZE) {
		return -1;
	}
	if (VIRTIO_REG(VIRTIO_MMIO_QUEUE_PFN) != 0) {
		return -1;
	}

	paddr_t pa1 = pmm_alloc();
	paddr_t pa2 = pmm_alloc();
	if (pa1 == PMM_INVALID || pa2 == PMM_INVALID) {
		if (pa1 != PMM_INVALID) {
			pmm_free(pa1);
		}
		if (pa2 != PMM_INVALID) {
			pmm_free(pa2);
		}
		return -1;
	}
	paddr_t base_pa;
	if (pa2 == pa1 + PAGE_SIZE) {
		base_pa = pa1;
	} else if (pa1 == pa2 + PAGE_SIZE) {
		base_pa = pa2;
	} else {
		pmm_free(pa1);
		pmm_free(pa2);
		return -1;
	}

	char *base = PA_TO_VA(base_pa);
	desc = (struct virtq_desc *)base;
	avail = (struct virtq_avail *)(base + sizeof(struct virtq_desc) * VIRTIO_QUEUE_SIZE);
	used = (struct virtq_used *)(base + PAGE_SIZE);

	VIRTIO_REG(VIRTIO_MMIO_QUEUE_NUM) = VIRTIO_QUEUE_SIZE;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_ALIGN) = PAGE_SIZE;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_PFN) = base_pa / PAGE_SIZE;

	for (int i = 0; i < VIRTIO_QUEUE_SIZE - 1; i++) {
		desc[i].next = i + 1;
	}
	desc[VIRTIO_QUEUE_SIZE - 1].next = 0xFFFF;
	free_head = 0;

	avail->flags = 0;
	avail->idx = 0;

	return 0;
}

int virtio_probe(void) {
	if (VIRTIO_REG(VIRTIO_MMIO_MAGIC) != VIRTIO_MAGIC_VALUE) {
		return -1;
	}
	if (VIRTIO_REG(VIRTIO_MMIO_VERSION) != VIRTIO_VERSION_LEGACY) {
		return -1;
	}
	if (VIRTIO_REG(VIRTIO_MMIO_DEVICE_ID) != VIRTIO_DEVICE_ID_BLOCK) {
		return -1;
	}
	return 0;
}

void virtio_reset(void) {
	VIRTIO_REG(VIRTIO_MMIO_STATUS) = 0;
}

void virtio_init(void) {
	if (virtio_probe() < 0) {
		kprintf("virtio: no block device\n");
		return;
	}
	unsigned int vendor = VIRTIO_REG(VIRTIO_MMIO_VENDOR_ID);

	virtio_reset();

	VIRTIO_REG(VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_ACKNOWLEDGE;
	VIRTIO_REG(VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

	VIRTIO_REG(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 0;
	(void)VIRTIO_REG(VIRTIO_MMIO_DEVICE_FEATURES);
	VIRTIO_REG(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
	VIRTIO_REG(VIRTIO_MMIO_DRIVER_FEATURES) = 0;

	volatile unsigned int *cfg =
	    (volatile unsigned int *)(VIRTIO0_VA + VIRTIO_MMIO_CONFIG);
	unsigned long capacity = cfg[0] | ((unsigned long)cfg[1] << 32);

	if (virtio_queue_init() < 0) {
		kprintf("virtio: queue init failed\n");
		return;
	}

	last_used_idx = 0;

	dsb();

	VIRTIO_REG(VIRTIO_MMIO_STATUS) =
	    VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK;

	kprintf("virtio-blk: capacity = %lu sectors (vendor %x)\n", capacity, vendor);
}

void virtio_init_irq(void) {
	gic_enable_irq(VIRTIO_IRQ);
}

void virtio_intr(void) {
	unsigned int status = VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_STATUS);
	VIRTIO_REG(VIRTIO_MMIO_INTERRUPT_ACK) = status;

	while (last_used_idx != used->idx) {
		last_used_idx++;
	}

	proc_wakeup(&virtio_disk_chan);
}

static int virtio_disk_rw(unsigned long sector, void *buf, int write) {
	if ((VIRTIO_REG(VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_DRIVER_OK) == 0) {
		return VIRTIO_E_RESET;
	}

	int idx[3];
	for (int i = 0; i < 3; i++) {
		idx[i] = alloc_desc();
		if (idx[i] < 0) {
			for (int j = 0; j < i; j++) {
				free_desc(idx[j]);
			}
			return VIRTIO_E_IOERR;
		}
	}

	blk_hdr.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
	blk_hdr.reserved = 0;
	blk_hdr.sector = sector;

	desc[idx[0]].addr = VA_TO_PA((paddr_t)&blk_hdr);
	desc[idx[0]].len = sizeof(blk_hdr);
	desc[idx[0]].flags = VIRTQ_DESC_F_NEXT;
	desc[idx[0]].next = idx[1];

	desc[idx[1]].addr = VA_TO_PA((paddr_t)buf);
	desc[idx[1]].len = 512;
	desc[idx[1]].flags = VIRTQ_DESC_F_NEXT;
	if (!write) {
		desc[idx[1]].flags |= VIRTQ_DESC_F_WRITE;
	}
	desc[idx[1]].next = idx[2];

	desc[idx[2]].addr = VA_TO_PA((paddr_t)&blk_status);
	desc[idx[2]].len = 1;
	desc[idx[2]].flags = VIRTQ_DESC_F_WRITE;
	desc[idx[2]].next = 0;

	blk_status = 0xFF;

	avail->ring[avail->idx % VIRTIO_QUEUE_SIZE] = idx[0];
	dsb();
	avail->idx++;
	dsb();

	unsigned short last_used = used->idx;
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

	if (current) {
		unsigned long deadline = timer_get_ticks() + VIRTIO_TIMEOUT_TICKS;
		while (used->idx == last_used) {
			unsigned long now = timer_get_ticks();
			if (now >= deadline) {
				for (int i = 0; i < 3; i++) {
					free_desc(idx[i]);
				}
				return VIRTIO_E_TIMEOUT;
			}
			proc_wait_timeout_nointr(&virtio_disk_chan, deadline - now);
		}
	} else {
		unsigned long timeout = 100000000;
		while (used->idx == last_used) {
			if (--timeout == 0) {
				for (int i = 0; i < 3; i++) {
					free_desc(idx[i]);
				}
				return VIRTIO_E_TIMEOUT;
			}
			nop();
		}
	}

	for (int i = 0; i < 3; i++) {
		free_desc(idx[i]);
	}

	if (blk_status == VIRTIO_BLK_S_OK) {
		return VIRTIO_E_OK;
	} else if (blk_status == VIRTIO_BLK_S_UNSUPP) {
		return VIRTIO_E_UNSUPP;
	}
	return VIRTIO_E_IOERR;
}

int virtio_disk_read(unsigned long sector, void *buf) {
	for (int retry = 0; retry < VIRTIO_MAX_RETRIES; retry++) {
		int ret = virtio_disk_rw(sector, buf, 0);
		if (ret != VIRTIO_E_IOERR) {
			return ret;
		}
	}
	return VIRTIO_E_IOERR;
}

int virtio_disk_write(unsigned long sector, void *buf) {
	for (int retry = 0; retry < VIRTIO_MAX_RETRIES; retry++) {
		int ret = virtio_disk_rw(sector, buf, 1);
		if (ret != VIRTIO_E_IOERR) {
			return ret;
		}
	}
	return VIRTIO_E_IOERR;
}
