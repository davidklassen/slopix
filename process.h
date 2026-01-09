#ifndef PROCESS_H
#define PROCESS_H

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
    unsigned long sp;   // Stack pointer
    unsigned long pc;   // Program counter (ELR_EL1)
    unsigned long pstate; // Processor state (SPSR_EL1)
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

// Process management functions
void process_init(void);
process_t *process_create(void (*entry)(void), unsigned long stack_size);
void process_exit(void);
process_t *process_get_current(void);
void process_set_current(process_t *proc);

#endif
