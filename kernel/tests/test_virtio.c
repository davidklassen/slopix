#ifdef RUN_TESTS

#include "test.h"
#include "virtio.h"

// M1: Device discovery (read-only, verify device is present)

TEST(virtio_magic_value) {
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_MAGIC), VIRTIO_MAGIC_VALUE, "Magic should be 0x74726976");
	return 0;
}

TEST(virtio_version_legacy) {
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_VERSION), VIRTIO_VERSION_LEGACY, "Version should be 1");
	return 0;
}

TEST(virtio_device_id_block) {
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_DEVICE_ID), VIRTIO_DEVICE_ID_BLOCK, "DeviceID should be 2");
	return 0;
}

TEST(virtio_vendor_id_qemu) {
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_VENDOR_ID), VIRTIO_VENDOR_ID_QEMU, "VendorID should be QEMU");
	return 0;
}

TEST_SUITE(virtio) {
	RUN_TEST(virtio_magic_value);
	RUN_TEST(virtio_version_legacy);
	RUN_TEST(virtio_device_id_block);
	RUN_TEST(virtio_vendor_id_qemu);
}

// M2: Feature negotiation (read-only, verify virtio_init set correct state)

TEST(virtio_status_initialized) {
	unsigned int expected = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_STATUS), expected, "Status should be ACKNOWLEDGE|DRIVER");
	return 0;
}

TEST(virtio_config_capacity) {
	volatile unsigned int *cfg =
	    (volatile unsigned int *)(VIRTIO0_VA + VIRTIO_MMIO_CONFIG);
	unsigned long capacity = cfg[0] | ((unsigned long)cfg[1] << 32);
	ASSERT(capacity > 0, "Capacity should be > 0");
	return 0;
}

TEST_SUITE(virtio_features) {
	RUN_TEST(virtio_status_initialized);
	RUN_TEST(virtio_config_capacity);
}

#endif
