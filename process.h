#ifndef PROCESS_H
#define PROCESS_H

// Process states
typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

// CPU context for process switching
typedef struct {
    unsigned long x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    unsigned long x29;  // Frame pointer
    unsigned long x30;  // Link register
    unsigned long sp;   // Stack pointer
    unsigned long pc;   // Program counter
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
