#include "virtio.h"
#include "kprintf.h"
#include "pmm.h"

static struct virtq_desc *desc;
static struct virtq_avail *avail;
static struct virtq_used *used;
static unsigned short free_head;

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
	if (pa1 == 0 || pa2 == 0) {
		if (pa1) {
			pmm_free(pa1);
		}
		if (pa2) {
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

	__asm__ volatile("dsb sy");

	VIRTIO_REG(VIRTIO_MMIO_STATUS) =
	    VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK;

	kprintf("virtio-blk: capacity = %lu sectors (vendor %x)\n", capacity, vendor);
}
