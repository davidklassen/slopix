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

// API
void virtio_init(void);
int virtio_probe(void);
void virtio_reset(void);

#endif
