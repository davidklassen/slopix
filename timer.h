#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// System register read/write macros
#define READ_SYSREG(reg, val) \
    __asm__ volatile("mrs %0, " #reg : "=r"(val))

#define WRITE_SYSREG(reg, val) \
    __asm__ volatile("msr " #reg ", %0" :: "r"(val))

// ARM Generic Timer System Registers
// CNTFRQ_EL0 - Counter Frequency Register (read-only)
static inline uint64_t read_cntfrq_el0(void) {
    uint64_t val;
    READ_SYSREG(cntfrq_el0, val);
    return val;
}

// CNTVCT_EL0 - Virtual Counter Register (read-only)
static inline uint64_t read_cntvct_el0(void) {
    uint64_t val;
    READ_SYSREG(cntvct_el0, val);
    return val;
}

// CNTV_CTL_EL0 - Virtual Timer Control Register
static inline uint64_t read_cntv_ctl_el0(void) {
    uint64_t val;
    READ_SYSREG(cntv_ctl_el0, val);
    return val;
}

static inline void write_cntv_ctl_el0(uint64_t val) {
    WRITE_SYSREG(cntv_ctl_el0, val);
}

// CNTV_CVAL_EL0 - Virtual Timer CompareValue Register
static inline uint64_t read_cntv_cval_el0(void) {
    uint64_t val;
    READ_SYSREG(cntv_cval_el0, val);
    return val;
}

static inline void write_cntv_cval_el0(uint64_t val) {
    WRITE_SYSREG(cntv_cval_el0, val);
}

// CNTV_TVAL_EL0 - Virtual Timer TimerValue Register
static inline uint64_t read_cntv_tval_el0(void) {
    uint64_t val;
    READ_SYSREG(cntv_tval_el0, val);
    return val;
}

static inline void write_cntv_tval_el0(uint64_t val) {
    WRITE_SYSREG(cntv_tval_el0, val);
}

// CNTV_CTL_EL0 bit definitions
#define CNTV_CTL_ENABLE   (1 << 0)  // Timer enable
#define CNTV_CTL_IMASK    (1 << 1)  // Timer interrupt mask
#define CNTV_CTL_ISTATUS  (1 << 2)  // Timer interrupt status

// Virtual timer interrupt number (GICv2 PPI)
#define TIMER_IRQ 27

// Function declarations
void timer_init(void);
uint64_t timer_get_frequency(void);
uint64_t timer_get_counter(void);
void timer_enable_irq(void);

// Periodic timer support
void timer_set_quantum(uint64_t ticks);
void timer_reload(void);
void timer_stop_periodic(void);
int timer_is_periodic(void);

#endif
