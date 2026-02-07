#include "rtc.h"
#include "kprintf.h"

void rtc_init(void) {
	unsigned int id0 = RTC_REG(RTC_PERIPHID0);
	unsigned int id1 = RTC_REG(RTC_PERIPHID1);
	unsigned int part = (id0 & 0xFF) | ((id1 & 0x0F) << 8);
	unsigned int designer = (id1 >> 4) & 0x0F;

	if (part != 0x031 || designer != 0x01) {
		kprintf("rtc: unknown device (part=0x%x designer=0x%x)\n", part, designer);
		return;
	}

	if (!(RTC_REG(RTC_CR) & 1)) {
		RTC_REG(RTC_CR) = 1;
	}

	kprintf("rtc: PL031 initialized, time=%lu\n", (unsigned long)rtc_read());
}

time_t rtc_read(void) {
	return (time_t)RTC_REG(RTC_DR);
}
