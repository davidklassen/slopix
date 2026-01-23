#ifdef RUN_TESTS

#include "test.h"
#include "virtio.h"

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

TEST(virtio_reset_clears_status) {
	VIRTIO_REG(VIRTIO_MMIO_STATUS) = 1;
	virtio_reset();
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_STATUS), 0, "Status should be 0");
	return 0;
}

TEST_SUITE(virtio) {
	RUN_TEST(virtio_magic_value);
	RUN_TEST(virtio_version_legacy);
	RUN_TEST(virtio_device_id_block);
	RUN_TEST(virtio_vendor_id_qemu);
	RUN_TEST(virtio_reset_clears_status);
}

#endif
