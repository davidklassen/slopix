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
	kprintf("virtio: block device (vendor %x)\n", vendor);
}
