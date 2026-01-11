#ifndef PROCESS_H
#define PROCESS_H

// PSTATE mode bits [3:0]:
// 0x0 = EL0t (EL0 with SP_EL0)
// 0x5 = EL1h (EL1 with SP_EL1)
// IRQ enabled means PSTATE.I bit is 0 (interrupts not masked)

#define PSTATE_EL0T_IRQ_ENABLED 0x0  // EL0t mode, IRQ enabled
#define PSTATE_EL1H_IRQ_ENABLED 0x5  // EL1h mode, IRQ enabled
#define PSTATE_MODE_MASK        0xF  // Bits [3:0] = execution mode

// Process states
typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

// CPU context for process switching (must match exception stack frame)
typedef struct {
    unsigned long x0, x1, x2, x3, x4, x5, x6, x7;
    unsigned long x8, x9, x10, x11, x12, x13, x14, x15;
    unsigned long x16, x17, x18, x19, x20, x21, x22, x23;
    unsigned long x24, x25, x26, x27, x28, x29, x30;
    unsigned long sp_el1;   // Stack pointer (EL1/kernel)
    unsigned long pc;   // Program counter (ELR_EL1)
    unsigned long pstate; // Processor state (SPSR_EL1)
    unsigned long sp_el0;        // User stack pointer (EL0)
    unsigned long ttbr0_el1;     // Per-process page table base
    unsigned char exception_level; // 0=EL0, 1=EL1
    unsigned char _padding[7];   // Align to 8-byte boundary
} cpu_context_t;

// Process control block
typedef struct process {
    int pid;
    process_state_t state;
    cpu_context_t context;
    void *stack;
    unsigned long stack_size;
    struct process *next;
} process_t;

// Helper functions to check process execution level
static inline int process_is_el0(const process_t *proc) {
    return proc->context.exception_level == 0;
}

static inline int process_is_el1(const process_t *proc) {
    return proc->context.exception_level == 1;
}

// Context frame size for exception handling
// Frame contains: x2, xzr, x3-x30, sp_el1, pc, pstate, x0, x1, sp_el0, ttbr0_el1
// Total: 34 original registers + 2 new fields = 36 quad-words
#define CONTEXT_FRAME_SIZE 36        // Total registers in context frame
#define CONTEXT_FRAME_BYTES (CONTEXT_FRAME_SIZE * 8)  // 288 bytes

// Context frame layout (36 quad-words = 288 bytes):
// [x2, xzr, x3, x4, ..., x30, sp_el1, pc, pstate, x0, x1, sp_el0, ttbr0_el1]
// Note: Frame size must be 16-byte aligned for ARM64 AAPCS64

// Process management functions
void process_init(void);
process_t *process_create(void (*entry)(void), unsigned long stack_size);
process_t *process_create_user(void (*entry)(void), unsigned long user_stack_size);
void process_exit(void);
process_t *process_get_current(void);
void process_set_current(process_t *proc);

#endif
