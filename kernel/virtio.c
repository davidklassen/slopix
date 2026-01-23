#include "virtio.h"
#include "kprintf.h"

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

	kprintf("virtio-blk: capacity = %lu sectors (vendor %x)\n", capacity, vendor);
}
