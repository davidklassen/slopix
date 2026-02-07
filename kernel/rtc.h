#ifndef RTC_H
#define RTC_H

#include "board.h"

#define RTC_REG(off) (*(volatile unsigned int *)(RTC_VA + (off)))

// Data register (current counter value)
#define RTC_DR 0x000
// Match register
#define RTC_MR 0x004
// Load register
#define RTC_LR 0x008
// Control register
#define RTC_CR 0x00C
// Interrupt mask set/clear
#define RTC_IMSC 0x010
// Raw interrupt status
#define RTC_RIS 0x014
// Masked interrupt status
#define RTC_MIS 0x018
// Interrupt clear
#define RTC_ICR 0x01C

// PrimeCell identification
#define RTC_PERIPHID0 0xFE0
#define RTC_PERIPHID1 0xFE4
#define RTC_PERIPHID2 0xFE8
#define RTC_PERIPHID3 0xFEC
#define RTC_PCELLID0  0xFF0
#define RTC_PCELLID1  0xFF4
#define RTC_PCELLID2  0xFF8
#define RTC_PCELLID3  0xFFC

typedef long time_t;

void rtc_init(void);
time_t rtc_read(void);

#endif
