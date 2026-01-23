#ifdef RUN_TESTS

#include "test.h"
#include "virtio.h"
#include "gic.h"
#include "board.h"

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

#define SCRATCH_SECTOR_1 100
#define SCRATCH_SECTOR_2 101
#define SCRATCH_SECTOR_3 102

static unsigned char write_buf[512];

TEST(virtio_write_read_verify) {
	for (int i = 0; i < 512; i++) {
		write_buf[i] = (unsigned char)(i & 0xFF);
	}
	int ret = virtio_disk_write(SCRATCH_SECTOR_1, write_buf);
	ASSERT_EQ(ret, 0, "Write should succeed");

	for (int i = 0; i < 512; i++) {
		write_buf[i] = 0;
	}
	ret = virtio_disk_read(SCRATCH_SECTOR_1, write_buf);
	ASSERT_EQ(ret, 0, "Read should succeed");

	for (int i = 0; i < 512; i++) {
		if (write_buf[i] != (unsigned char)(i & 0xFF)) {
			ASSERT(0, "Data mismatch after write-read");
		}
	}
	return 0;
}

TEST(virtio_write_multiple) {
	unsigned long sectors[] = {SCRATCH_SECTOR_1, SCRATCH_SECTOR_2, SCRATCH_SECTOR_3};
	for (int s = 0; s < 3; s++) {
		for (int i = 0; i < 512; i++) {
			write_buf[i] = (unsigned char)(s + 1);
		}
		int ret = virtio_disk_write(sectors[s], write_buf);
		ASSERT_EQ(ret, 0, "Write should succeed");
	}
	for (int s = 0; s < 3; s++) {
		int ret = virtio_disk_read(sectors[s], write_buf);
		ASSERT_EQ(ret, 0, "Read should succeed");
		ASSERT_EQ(write_buf[0], (unsigned char)(s + 1), "Data should match written pattern");
	}
	return 0;
}

TEST_SUITE(virtio_write) {
	RUN_TEST(virtio_write_read_verify);
	RUN_TEST(virtio_write_multiple);
}

TEST(virtio_intr_enabled) {
	unsigned int reg_off = GICD_ISENABLER0_OFF + 4 * (VIRTIO_IRQ / 32);
	unsigned int bit = VIRTIO_IRQ % 32;
	volatile unsigned int *reg = (volatile unsigned int *)(GICD_VA + reg_off);
	ASSERT((*reg & (1u << bit)) != 0, "VIRTIO_IRQ should be enabled in GIC");
	return 0;
}

TEST(virtio_intr_fires) {
	unsigned char buf[512];
	int ret = virtio_disk_read(0, buf);
	ASSERT_EQ(ret, 0, "Interrupt-driven read should succeed");
	ASSERT_EQ(buf[0], 'S', "Should read correct data");
	return 0;
}

TEST_SUITE(virtio_intr) {
	RUN_TEST(virtio_intr_enabled);
	RUN_TEST(virtio_intr_fires);
}

static unsigned char error_test_buf[512];

TEST(virtio_read_bad_sector) {
	int ret = virtio_disk_read(3000, error_test_buf);
	ASSERT(ret < 0, "Reading invalid sector should fail");
	return 0;
}

TEST(virtio_status_check) {
	int ret = virtio_disk_read(3000, error_test_buf);
	ASSERT_EQ(ret, VIRTIO_E_IOERR, "Invalid sector should return IOERR");
	return 0;
}

TEST_SUITE(virtio_errors) {
	RUN_TEST(virtio_read_bad_sector);
	RUN_TEST(virtio_status_check);
}

#endif
