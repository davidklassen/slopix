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
	unsigned int required = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
	unsigned int status = VIRTIO_REG(VIRTIO_MMIO_STATUS);
	ASSERT((status & required) == required, "Status should have ACKNOWLEDGE|DRIVER");
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

// M3: Virtqueue setup (read-only, verify virtio_init configured queue)

TEST(virtio_queue_num_max) {
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_SEL) = 0;
	unsigned int num_max = VIRTIO_REG(VIRTIO_MMIO_QUEUE_NUM_MAX);
	ASSERT(num_max > 0, "QueueNumMax should be > 0");
	return 0;
}

TEST(virtio_queue_alloc) {
	unsigned int status = VIRTIO_REG(VIRTIO_MMIO_STATUS);
	ASSERT_EQ(status & VIRTIO_STATUS_DRIVER_OK, VIRTIO_STATUS_DRIVER_OK, "DRIVER_OK implies queue alloc succeeded");
	return 0;
}

TEST(virtio_queue_pfn_written) {
	VIRTIO_REG(VIRTIO_MMIO_QUEUE_SEL) = 0;
	unsigned int pfn = VIRTIO_REG(VIRTIO_MMIO_QUEUE_PFN);
	ASSERT(pfn != 0, "QueuePFN should be non-zero after setup");
	return 0;
}

TEST(virtio_status_driver_ok) {
	unsigned int expected =
	    VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK;
	ASSERT_EQ(VIRTIO_REG(VIRTIO_MMIO_STATUS), expected, "Status should be ACKNOWLEDGE|DRIVER|DRIVER_OK");
	return 0;
}

TEST_SUITE(virtio_queue) {
	RUN_TEST(virtio_queue_num_max);
	RUN_TEST(virtio_queue_alloc);
	RUN_TEST(virtio_queue_pfn_written);
	RUN_TEST(virtio_status_driver_ok);
}

// M4: Block read (polling)

static unsigned char read_buf[512];

TEST(virtio_read_sector_zero) {
	int ret = virtio_disk_read(0, read_buf);
	ASSERT_EQ(ret, 0, "Read should succeed");
	return 0;
}

TEST(virtio_read_returns_data) {
	int ret = virtio_disk_read(0, read_buf);
	ASSERT_EQ(ret, 0, "Read should succeed");
	ASSERT_EQ(read_buf[0], 'S', "First byte should be 'S'");
	ASSERT_EQ(read_buf[1], 'L', "Second byte should be 'L'");
	ASSERT_EQ(read_buf[2], 'P', "Third byte should be 'P'");
	ASSERT_EQ(read_buf[3], 'X', "Fourth byte should be 'X'");
	return 0;
}

TEST(virtio_read_multiple) {
	for (unsigned long s = 0; s < 3; s++) {
		int ret = virtio_disk_read(s, read_buf);
		ASSERT_EQ(ret, 0, "Read should succeed");
	}
	return 0;
}

TEST_SUITE(virtio_read) {
	RUN_TEST(virtio_read_sector_zero);
	RUN_TEST(virtio_read_returns_data);
	RUN_TEST(virtio_read_multiple);
}

#endif
