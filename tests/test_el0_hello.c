#include "test_framework.h"
#include "../syscall.h"
#include "../process.h"
#include "../scheduler.h"
#include "../printf.h"

static volatile int el0_test_completed = 0;

// Global flag visible to syscall.c - not static
volatile int is_demo_process = 0;

// User program - runs at EL0
static void user_hello_world(void) {
    // Message to print
    const char *msg = "Hello from EL0!\n";

    // Syscall: write(1, msg, 17)
    __asm__ volatile(
        "mov x8, %0\n"      // SYS_write
        "mov x0, #1\n"      // fd = stdout
        "mov x1, %1\n"      // buf
        "mov x2, #17\n"     // count
        "svc #0\n"          // Syscall (EL0→EL1)
        :
        : "i"(SYS_write), "r"(msg)
        : "x0", "x1", "x2", "x8"
    );

    // Syscall: getpid()
    __asm__ volatile(
        "mov x8, %0\n"      // SYS_getpid
        "svc #0\n"          // Syscall
        :
        : "i"(SYS_getpid)
        : "x0", "x8"
    );

    // PID is now in x0, but we'll just print a success message
    const char *msg2 = "Syscalls working!\n";
    __asm__ volatile(
        "mov x8, %0\n"
        "mov x0, #1\n"
        "mov x1, %1\n"
        "mov x2, #18\n"
        "svc #0\n"
        :
        : "i"(SYS_write), "r"(msg2)
        : "x0", "x1", "x2", "x8"
    );

    // Syscall: exit(0)
    __asm__ volatile(
        "mov x8, %0\n"      // SYS_exit
        "mov x0, #0\n"      // status = 0
        "svc #0\n"          // Should not return
        :
        : "i"(SYS_exit)
        : "x0", "x8"
    );

    // Should never reach here
    while (1) __asm__ volatile("wfe");
}

static void test_el0_execution(void) {
    TEST("First EL0 process execution with syscalls");

    printf("  Note: This test validates EL0 process setup.\n");
    printf("  Actual EL0 execution requires interrupt/timer-driven scheduling.\n");

    el0_test_completed = 0;

    // Create EL0 process
    process_t *proc = process_create_user(user_hello_world, 8192);
    ASSERT(proc != 0, "EL0 process created");

    if (!proc) return;

    // Verify it's configured as EL0
    ASSERT_EQ(proc->context.exception_level, 0);
    ASSERT_EQ(proc->context.pstate, PSTATE_EL0T_IRQ_ENABLED);

    printf("  EL0 process PID=%d configured correctly\n", proc->pid);
    printf("    PC (entry point): 0x%lx\n", proc->context.pc);
    printf("    User stack (SP_EL0): 0x%lx\n", proc->context.sp_el0);
    printf("    Kernel stack (SP_EL1): 0x%lx\n", proc->context.sp_el1);
    printf("    PSTATE: 0x%lx (EL0t mode)\n", proc->context.pstate);

    // Verify stacks are properly set up
    ASSERT(proc->context.sp_el0 != 0, "User stack allocated");
    ASSERT(proc->context.sp_el1 != 0, "Kernel stack allocated");
    ASSERT(proc->context.sp_el0 != proc->context.sp_el1, "Dual stacks are separate");

    // Verify alignment
    ASSERT_EQ(proc->context.sp_el0 & 0xF, 0);
    ASSERT_EQ(proc->context.sp_el1 & 0xF, 0);

    printf("  " COLOR_GREEN "EL0 process setup validated!" COLOR_RESET "\n");
    printf("  " COLOR_YELLOW "Note: Full execution test requires timer-driven scheduler" COLOR_RESET "\n");
}

static void test_el0_with_el1_processes(void) {
    TEST("EL0 and EL1 processes have distinct configurations");

    // Dummy processes for testing
    void dummy_el1(void) { process_exit(); }
    void dummy_el0(void) {
        __asm__ volatile("mov x8, %0\n mov x0, #0\n svc #0\n" :: "i"(SYS_exit) : "x0", "x8");
    }

    // Create both types
    process_t *proc_el1 = process_create(dummy_el1, 4096);
    process_t *proc_el0 = process_create_user(dummy_el0, 4096);

    ASSERT(proc_el1 != 0 && proc_el0 != 0, "Both processes created");

    // Verify EL1 process configuration
    ASSERT_EQ(proc_el1->context.exception_level, 1);
    ASSERT_EQ(proc_el1->context.pstate, PSTATE_EL1H_IRQ_ENABLED);
    ASSERT_EQ(proc_el1->context.sp_el0, 0);  // No user stack

    // Verify EL0 process configuration
    ASSERT_EQ(proc_el0->context.exception_level, 0);
    ASSERT_EQ(proc_el0->context.pstate, PSTATE_EL0T_IRQ_ENABLED);
    ASSERT(proc_el0->context.sp_el0 != 0, "EL0 has user stack");
    ASSERT(proc_el0->context.sp_el1 != 0, "EL0 has kernel stack");

    // Verify they can be added to scheduler together
    scheduler_add(proc_el1);
    scheduler_add(proc_el0);

    printf("  EL1 process: exception_level=%d, pstate=0x%lx, sp_el0=%lx\n",
           proc_el1->context.exception_level, proc_el1->context.pstate, proc_el1->context.sp_el0);
    printf("  EL0 process: exception_level=%d, pstate=0x%lx, sp_el0=%lx, sp_el1=%lx\n",
           proc_el0->context.exception_level, proc_el0->context.pstate,
           proc_el0->context.sp_el0, proc_el0->context.sp_el1);

    printf("  " COLOR_GREEN "EL0 and EL1 processes coexist with distinct configs!" COLOR_RESET "\n");
}

// Actual EL0 demonstration - runs after all tests pass
void demonstrate_el0_execution(void) {
    printf("\n" COLOR_YELLOW "=== EL0 Execution Demonstration ===" COLOR_RESET "\n");
    printf("Now running actual EL0 process with syscalls...\n\n");

    el0_test_completed = 0;
    is_demo_process = 1;  // Signal that this is the demo process

    // Create EL0 process
    process_t *proc = process_create_user(user_hello_world, 8192);
    if (!proc) {
        printf("[ERROR] Failed to create EL0 process\n");
        return;
    }

    printf("[DEMO] Created EL0 process PID=%d\n", proc->pid);
    printf("[DEMO] Entry point (PC): 0x%lx\n", proc->context.pc);
    printf("[DEMO] User stack (SP_EL0): 0x%lx\n", proc->context.sp_el0);
    printf("[DEMO] Kernel stack (SP_EL1): 0x%lx\n", proc->context.sp_el1);
    printf("[DEMO] PSTATE: 0x%lx (EL0t mode, IRQ enabled)\n", proc->context.pstate);
    printf("[DEMO] Exception level: %d (EL0)\n\n", proc->context.exception_level);

    // Add to scheduler
    scheduler_add(proc);
    process_set_current(proc);

    printf("[DEMO] Transitioning to EL0...\n");
    printf("[DEMO] Process will execute user_hello_world() at EL0\n");
    printf("[DEMO] Syscalls will transition EL0→EL1→EL0\n\n");

    // Manually transition to EL0 using ERET
    // This simulates what the exception handler restore would do
    __asm__ volatile(
        // Set up user stack pointer (SP_EL0)
        "msr sp_el0, %0\n"
        // Set up return address (ELR_EL1 = entry point)
        "msr elr_el1, %1\n"
        // Set up processor state (SPSR_EL1 = EL0t mode)
        "msr spsr_el1, %2\n"
        // Exception return - transitions to EL0
        "eret\n"
        :
        : "r"(proc->context.sp_el0),
          "r"(proc->context.pc),
          "r"(proc->context.pstate)
        : "memory"
    );

    // Never reached - process calls exit() which enters WFE loop
}

void run_el0_hello_tests(void) {
    TEST_SUITE("EL0 Execution");
    test_el0_execution();
    test_el0_with_el1_processes();
}
