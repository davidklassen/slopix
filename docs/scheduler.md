# ARM64 Scheduler Implementation for Slopix

## Table of Contents
1. [Overview](#overview)
2. [ARM64 Exception Model](#arm64-exception-model)
3. [Context Switching Fundamentals](#context-switching-fundamentals)
4. [ARM Generic Timer](#arm-generic-timer)
5. [Generic Interrupt Controller (GIC)](#generic-interrupt-controller-gic)
6. [Interrupt Masking (PSTATE.DAIF)](#interrupt-masking-pstatedaif)
7. [Current Implementation Status](#current-implementation-status)
8. [Missing Components](#missing-components)
9. [Implementation Roadmap](#implementation-roadmap)
10. [References](#references)

---

## Overview

This document provides a comprehensive guide to implementing a preemptive scheduler for the Slopix operating system on ARM64 (AArch64) architecture. The scheduler uses timer interrupts to implement time-sliced, round-robin multitasking between kernel-level threads running at EL1 (Exception Level 1).

### Goals
- Implement preemptive multitasking with automatic context switching
- Use ARM Generic Timer to trigger periodic scheduler invocations
- Support both kernel threads (EL1) and user processes (EL0)
- Maintain clean separation between architecture-specific and generic code

---

## ARM64 Exception Model

### Exception Levels

ARM64 defines four exception levels with increasing privilege:
- **EL0**: Unprivileged user applications
- **EL1**: Operating system kernel (Slopix runs here)
- **EL2**: Hypervisor
- **EL3**: Secure monitor firmware

*Source: [AArch64 Exception Levels](https://medium.com/@om.nara/aarch64-exception-levels-60d3a74280e6)*

### Key Properties
1. **Mandatory levels**: EL0 and EL1 must be implemented in all ARM64 processors
2. **Privilege**: Higher levels can access all resources of lower levels
3. **Transitions**: Moving from lower to higher EL only occurs via exceptions
4. **Return**: ERET instruction returns to the exception level stored in SPSR_ELn

*Source: [ARMv-8a Exception Levels](https://pyjamabrah.com/posts/arm64-day0-exception-levels/)*

### Exception Types

Each exception level has its own vector table (VBAR_EL1 for EL1):

```
Offset    Exception Type              Source EL
------    ---------------             ---------
0x000     Synchronous (Current EL)    EL1 → EL1
0x080     IRQ (Current EL)            EL1 → EL1
0x100     FIQ (Current EL)            EL1 → EL1
0x180     SError (Current EL)         EL1 → EL1
0x200     Synchronous (Lower EL)      EL0 → EL1
0x280     IRQ (Lower EL)              EL0 → EL1
0x300     FIQ (Lower EL)              EL0 → EL1
0x380     SError (Lower EL)           EL0 → EL1
```

*Source: [Learn the Architecture - AArch64 Exception Model](https://documentation-service.arm.com/static/63a065c41d698c4dc521cb1c)*

### Automatic State Saving

When an exception occurs:
1. **Program counter** saved to **ELR_EL1** (Exception Link Register)
2. **Processor state** (PSTATE) saved to **SPSR_EL1** (Saved Program Status Register)
3. **ESR_EL1** (Exception Syndrome Register) records the exception cause
4. Execution jumps to the appropriate vector in the exception vector table

*Source: [AArch64 Exception Model - ARM](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Exception%20model.pdf)*

---

## Context Switching Fundamentals

### What is Context Switching?

Context switching is the process of saving the state of a currently running process and restoring the state of the next process to run. This allows multiple processes to share a single CPU.

*Source: [Lab 4: Preemptive Multitasking](https://tc.gts3.org/cs3210/2020/spring/lab/lab4.html)*

### Context Components

A complete context includes:
1. **General-purpose registers**: x0-x30 (31 registers)
2. **Stack pointers**: SP_EL1 (kernel), SP_EL0 (user)
3. **Program counter**: Saved in ELR_EL1 during exceptions
4. **Processor state**: SPSR_EL1 (PSTATE flags)
5. **Page table**: TTBR0_EL1 (per-process address space)

### Slopix Context Frame Layout

```
Offset   Register        Purpose
------   --------        -------
0        sp_el0          User stack pointer
8        ttbr0_el1       Process page table
16       x2              General purpose
24       xzr             Zero register (placeholder)
32       x3-x28          General purpose registers
...
240      x29             Frame pointer
248      x30             Link register
256      ELR_EL1         Saved program counter
264      SPSR_EL1        Saved processor state
272      x0              Return value / syscall arg
280      x1              Syscall arg

Total size: 288 bytes (36 quad-words)
```

### Context Switch Types

1. **Voluntary**: Process calls a syscall (e.g., `sys_exit()`, `sys_yield()`)
2. **Preemptive**: Timer interrupt forces switch (not yet implemented in Slopix)
3. **Blocked**: Process waits for I/O or event (future enhancement)

*Source: [raspberry-pi-os Scheduler Tutorial](https://s-matyukevich.github.io/raspberry-pi-os/docs/lesson04/rpi-os.html)*

---

## ARM Generic Timer

### Overview

The ARM Generic Timer is a system-wide timer that provides time-keeping services to all cores. It's essential for implementing preemptive scheduling.

*Source: [AArch64 Generic Timer Guide](https://tc.gts3.org/cs3210/2020/spring/r/aarch64-generic-timer.pdf)*

### Timer Types

ARM64 provides multiple timer types:
1. **Physical Timer** (EL1): CNTP_CTL_EL0, CNTP_CVAL_EL0, CNTP_TVAL_EL0
2. **Virtual Timer** (EL1): CNTV_CTL_EL0, CNTV_CVAL_EL0, CNTV_TVAL_EL0
3. **Hypervisor Timer** (EL2): CNTHP_CTL_EL2, CNTHP_CVAL_EL2, CNTHP_TVAL_EL2

**Recommendation**: Use the **virtual timer** (CNTV) for operating systems at EL1. This allows a hypervisor at EL2 to virtualize time.

### Key Registers

#### CNTV_CTL_EL0 - Virtual Timer Control Register
```
Bits    Field       Description
----    -----       -----------
[0]     ENABLE      Timer enable (0=disabled, 1=enabled)
[1]     IMASK       Interrupt mask (0=enabled, 1=masked)
[2]     ISTATUS     Interrupt status (1=condition met)
```

*Source: [CNTV_CTL_EL0 Register](https://developer.arm.com/documentation/ddi0595/2021-06/AArch64-Registers/DAIF--Interrupt-Mask-Bits)*

**Important**: An interrupt is generated when `ISTATUS=1` AND `IMASK=0`

#### CNTV_TVAL_EL0 - Virtual Timer TimerValue Register
- 32-bit signed downcounter
- Counts down at the system counter frequency (typically 62.5 MHz on RPi)
- Writing to TVAL sets CNTV_CVAL = CNTVCT + TVAL

*Source: [CNTV_TVAL_EL0 Register](https://developer.arm.com/documentation/101550/latest/AArch64-registers/AArch64-register-descriptions/AArch64-Generic-Timer-register-description/CNTV-TVAL-EL0--Counter-timer-Virtual-Timer-TimerValue-Register)*

#### CNTV_CVAL_EL0 - Virtual Timer CompareValue Register
- 64-bit absolute compare value
- Interrupt triggered when CNTVCT_EL0 >= CNTV_CVAL_EL0

#### CNTVCT_EL0 - Virtual Counter Register
- 64-bit read-only counter
- Increments at system counter frequency
- Provides current time for comparison

### Timer Interrupt Mechanism

1. System counter (CNTVCT_EL0) continuously increments
2. When `CNTVCT_EL0 >= CNTV_CVAL_EL0`, timer condition is met
3. If timer is enabled (ENABLE=1) and not masked (IMASK=0), interrupt is generated
4. Interrupt is level-sensitive: persists until condition is cleared

*Source: [ARM Generic Timer Implementation](https://github.com/littlekernel/lk/blob/master/dev/timer/arm_generic/arm_generic_timer.c)*

### Timer Initialization Sequence

```c
// 1. Read current counter value
uint64_t current_count = read_sysreg(cntvct_el0);

// 2. Calculate compare value (e.g., 10ms at 62.5 MHz)
uint64_t ticks_per_10ms = 625000;  // 62.5 MHz * 0.01s
write_sysreg(cntv_cval_el0, current_count + ticks_per_10ms);

// Or use TVAL for simpler programming:
write_sysreg(cntv_tval_el0, 625000);  // 10ms

// 3. Enable timer and unmask interrupt
write_sysreg(cntv_ctl_el0, 0x1);  // ENABLE=1, IMASK=0
```

### Timer ISR Pattern

```c
void timer_irq_handler(void) {
    // 1. Reload timer for next interrupt (10ms quantum)
    write_sysreg(cntv_tval_el0, 625000);

    // 2. Acknowledge interrupt at GIC
    gic_end_interrupt(27);  // IRQ 27 is virtual timer

    // 3. Call scheduler
    scheduler_schedule_with_context(saved_context);
}
```

*Source: [OSv ARM Clock Implementation](https://github.com/cloudius-systems/osv/blob/master/arch/aarch64/arm-clock.cc)*

---

## Generic Interrupt Controller (GIC)

### Overview

The GIC (Generic Interrupt Controller) routes interrupts from peripherals to CPU cores. Understanding the GIC is essential for enabling timer interrupts.

*Source: [ARM GIC Architecture Specification](https://www.scs.stanford.edu/~zyedidia/docs/arm/gic_v3.pdf)*

### GIC Components

1. **Distributor (GICD)**: Centralizes interrupt control, prioritizes interrupts
2. **CPU Interface (GICC)**: Per-core interface for receiving interrupts (GICv2)
3. **Redistributor (GICR)**: Per-core configuration (GICv3 only)

*Source: [Bare Metal ARM Interrupts](https://github.com/umanovskis/baremetal-arm/blob/master/doc/07_interrupts.md)*

### Interrupt Types

1. **Software Generated Interrupts (SGI)**: IDs 0-15, inter-core communication
2. **Private Peripheral Interrupts (PPI)**: IDs 16-31, per-core peripherals (timers)
3. **Shared Peripheral Interrupts (SPI)**: IDs 32+, shared devices (UART, GPIO)

**ARM Generic Timer IRQs** (PPIs):
- IRQ 27 (INTID 27): Virtual timer (CNTV)
- IRQ 30 (INTID 30): Physical timer (CNTP)

*Source: [Linux kernel timer documentation](https://www.kernel.org/doc/Documentation/devicetree/bindings/timer/arm,arch_timer.txt)*

### GIC Initialization Sequence (GICv2)

```c
// 1. Enable distributor
GICD_CTLR |= 0x1;

// 2. Enable virtual timer interrupt (IRQ 27)
uint32_t reg_idx = 27 / 32;  // Register index
uint32_t bit_idx = 27 % 32;  // Bit within register
GICD_ISENABLER[reg_idx] |= (1 << bit_idx);

// 3. Set interrupt priority (lower = higher priority)
GICD_IPRIORITYR[27] = 0xA0;  // Mid priority

// 4. Route to CPU0
GICD_ITARGETSR[27] = 0x1;  // Target CPU0

// 5. Enable CPU interface
GICC_CTLR |= 0x1;

// 6. Set priority mask (allow all priorities)
GICC_PMR = 0xFF;
```

*Source: [Implementing Bare-Metal GICv3](https://www.systemonchips.com/implementing-bare-metal-gicv3-on-aarch64-irq-handling-and-timer-configuration-challenges/)*

### GIC Interrupt Acknowledgement Flow

```c
void irq_handler(void) {
    // 1. Read interrupt ID from IAR
    uint32_t irq_id = GICC_IAR & 0x3FF;

    // 2. Handle the interrupt
    if (irq_id == 27) {
        timer_irq_handler();
    }

    // 3. Signal end of interrupt
    GICC_EOIR = irq_id;
}
```

*Source: [Learn the Architecture - GIC](https://developer.arm.com/docs/den0024/latest/aarch64-exception-handling/the-generic-interrupt-controller)*

### GICv3 Differences

GICv3 requires additional steps:
1. Configure redistributor (GICR) for each core
2. Use system registers instead of memory-mapped GICC
3. Set ICC_PMR_EL1 and ICC_IGRPEN1_EL1

*Source: [Bare Metal GICv3 Implementation](https://www.systemonchips.com/implementing-bare-metal-gicv3-on-aarch64-irq-handling-and-timer-configuration-challenges/)*

---

## Interrupt Masking (PSTATE.DAIF)

### DAIF Register Overview

The DAIF system register controls interrupt masking at the processor level. DAIF is part of PSTATE (Processor State).

*Source: [DAIF: Interrupt Mask Bits](https://developer.arm.com/documentation/ddi0595/2021-06/AArch64-Registers/DAIF--Interrupt-Mask-Bits)*

### DAIF Bit Fields

```
Bit     Name    Description
---     ----    -----------
[9]     D       Debug exceptions mask
[8]     A       SError (asynchronous abort) mask
[7]     I       IRQ mask
[6]     F       FIQ mask
```

**Mask behavior**: When a bit is **1**, the corresponding interrupt is **masked** (disabled)

### Enabling Interrupts

Use the `MSR DAIFClr` instruction to **clear** (enable) interrupt bits:

```assembly
// Enable IRQs only
msr daifclr, #2        // Clear I bit (bit 1 of immediate = DAIF bit 7)

// Enable IRQs and FIQs
msr daifclr, #3        // Clear I and F bits
```

*Source: [AArch64 Interrupt Handling](https://krinkinmu.github.io/2021/01/10/aarch64-interrupt-handling.html)*

### Disabling Interrupts

Use the `MSR DAIFSet` instruction to **set** (disable) interrupt bits:

```assembly
// Disable IRQs only
msr daifset, #2        // Set I bit

// Disable all interrupts
msr daifset, #15       // Set all DAIF bits (0b1111)
```

### Reading DAIF

```assembly
// Read current DAIF value
mrs x0, daif

// Check if IRQs are masked
tst x0, #0x80          // Test bit 7 (I bit)
```

### Critical Sections

For protecting shared data structures:

```c
// Save and disable interrupts
uint64_t save_daif(void) {
    uint64_t daif;
    asm volatile("mrs %0, daif" : "=r"(daif));
    asm volatile("msr daifset, #2");  // Disable IRQs
    return daif;
}

// Restore previous interrupt state
void restore_daif(uint64_t daif) {
    asm volatile("msr daif, %0" : : "r"(daif));
}
```

### Important Considerations

1. **GIC Priority Mask**: Even with DAIF.I=0, interrupts are masked until `ICC_PMR_EL1` is set appropriately
2. **Exception Entry**: DAIF bits are automatically set when taking an exception
3. **Exception Return**: DAIF is restored from SPSR_EL1 on ERET

*Source: [Interrupts on RPI3 in aarch64 mode](https://forums.raspberrypi.com/viewtopic.php?t=188366)*

---

## Current Implementation Status

### ✅ Implemented Components

#### 1. Process Management (`process.c`, `process.h`)
- Process structure with state tracking (READY, RUNNING, BLOCKED, TERMINATED)
- `process_create()`: Creates EL1 kernel threads
- `process_create_user()`: Creates EL0 userspace processes
- Complete context structure (31 registers, SP_EL0, SP_EL1, PC, PSTATE, TTBR0)
- `process_exit()`: Marks process as TERMINATED

#### 2. Scheduler (`scheduler.c`, `scheduler.h`)
- Circular linked list run queue
- `scheduler_init()`: Initialize empty queue
- `scheduler_add()`: Add process to queue, set state to READY
- `scheduler_remove()`: Remove process from queue
- `scheduler_schedule_with_context()`: **Core scheduling function**
  - Implements round-robin algorithm
  - Saves current context to process structure
  - Loads next process context onto stack
  - Returns pointer to new stack for exception handler

#### 3. Exception Handling (`exceptions.S`, `interrupts.c`)
- Complete exception vector table at VBAR_EL1
- Separate handlers for Current EL and Lower EL (EL0→EL1)
- Full context save/restore (36 registers, 288 bytes)
- `exception_handler_sync()`: Synchronous exception handler
  - Saves context on stack
  - Calls `handle_sync_exception_with_context()`
  - Restores potentially new context (enables context switching)
- `el0_sync_handler()`, `el0_irq_handler()`: Lower EL handlers
  - Saves/restores SP_EL0 and TTBR0_EL1
  - TLB invalidation after context switch

#### 4. Syscall Interface (`syscall.c`, `syscall.h`)
- `sys_exit()`: Terminates process, calls `scheduler_remove()`
- `sys_write()`: UART output
- `sys_getpid()`: Return process ID
- Syscall dispatch via SVC instruction (synchronous exception)

#### 5. Memory Management
- Physical memory manager (PMM): Bitmap-based page allocator
- MMU configuration: TTBR0 (user), TTBR1 (kernel)
- Per-process page tables (TTBR0_EL1 in context)
- Stack allocation: 8KB kernel stack, 4-8KB user stack

#### 6. Test Infrastructure
- `test_scheduler.c`: Tests scheduler selection logic
- `test_trampoline.S`: Manual context switch testing
- Tests verify context frame layout and scheduler behavior

### ❌ Missing Components

#### 1. Timer Implementation (CRITICAL)
**Status**: No timer.c or timer.h files exist

**Required**:
- Timer initialization function
- ARM Generic Timer (CNTV) configuration
- Timer frequency detection (read CNTFRQ_EL0)
- Timer reload function for periodic interrupts
- Time quantum definition (10ms recommended)

**Functions needed**:
```c
void timer_init(void);
void timer_set_quantum_ms(uint32_t ms);
uint64_t timer_get_ticks(void);
void timer_reload(void);
```

#### 2. IRQ Handler Implementation
**Status**: IRQ vectors are stubs (`b .` infinite loop)

**Required**:
- IRQ context save (similar to sync handler)
- GIC interrupt acknowledgement
- Dispatch to timer handler
- Call scheduler if timer IRQ
- GIC end-of-interrupt signal

**File**: `exceptions.S` needs IRQ handlers implemented

#### 3. GIC Support
**Status**: No GIC initialization code exists

**Required**:
- GIC version detection (v2 vs v3)
- Distributor initialization
- CPU interface initialization
- Enable IRQ 27 (virtual timer)
- Set timer interrupt priority and routing

**Functions needed**:
```c
void gic_init(void);
void gic_enable_interrupt(uint32_t irq_id);
void gic_set_priority(uint32_t irq_id, uint32_t priority);
uint32_t gic_acknowledge_interrupt(void);
void gic_end_interrupt(uint32_t irq_id);
```

#### 4. Interrupt Enable/Disable
**Status**: `interrupts_enable()` and `interrupts_disable()` are no-ops

**Required**:
- Implement DAIF manipulation
- Critical sections for data structure protection
- Careful initialization ordering (enable after scheduler ready)

**Implementation**:
```c
void interrupts_enable(void) {
    asm volatile("msr daifclr, #2");  // Clear I bit
}

void interrupts_disable(void) {
    asm volatile("msr daifset, #2");  // Set I bit
}
```

#### 5. Process Lifecycle Management
**Status**: No automatic cleanup of TERMINATED processes

**Required**:
- Reap terminated processes
- Free stack memory
- Free process structure
- Remove from scheduler queue

**Function needed**:
```c
void process_reap(struct process *proc);
```

#### 6. Integration Between Timer and Scheduler
**Status**: No timer ISR calls scheduler

**Required**:
- Timer IRQ handler that invokes `scheduler_schedule_with_context()`
- Pass saved context from IRQ handler to scheduler
- Timer reload after each scheduler invocation

#### 7. Multi-Process Demonstration
**Status**: `thread1()` and `thread2()` defined but not instantiated

**Required**:
- Create process for thread1
- Create process for thread2
- Add both to scheduler
- Enable timer and interrupts
- Demonstrate preemptive switching

---

## Missing Components

This section is covered in "Current Implementation Status" above.

---

## Implementation Roadmap

### Phase 1: Timer Infrastructure (Foundation)

**Goal**: Get timer interrupts working without scheduler integration

#### Step 1.1: Timer Register Definitions
- Create `timer.h` with register access macros
- Define CNTV_CTL_EL0, CNTV_TVAL_EL0, CNTV_CVAL_EL0, CNTVCT_EL0, CNTFRQ_EL0
- Add read/write system register helpers

**Test**: Read and print CNTFRQ_EL0, verify timer frequency

#### Step 1.2: Basic Timer Functions
- Create `timer.c` with `timer_init()`
- Read counter frequency from CNTFRQ_EL0
- Calculate ticks per millisecond
- Implement `timer_set_quantum_ms(uint32_t ms)`

**Test**: Initialize timer, set 10ms quantum, verify TVAL register

#### Step 1.3: Timer Interrupt Enable (No GIC Yet)
- Set CNTV_CTL_EL0.ENABLE = 1
- Set CNTV_CTL_EL0.IMASK = 0
- Load CNTV_TVAL_EL0 with calculated ticks

**Test**: Spin in loop, verify CNTV_CTL_EL0.ISTATUS becomes 1

### Phase 2: GIC Support (Interrupt Routing)

**Goal**: Enable timer interrupts to trigger IRQ exception handler

#### Step 2.1: GIC Register Definitions
- Create `gic.h` with GICD and GICC base addresses
- Define register offsets (CTLR, ISENABLER, IPRIORITYR, etc.)
- Add GIC register access macros

**Test**: Read GICD_TYPER, verify GIC is accessible

#### Step 2.2: GIC Initialization
- Create `gic.c` with `gic_init()`
- Enable distributor (GICD_CTLR)
- Enable CPU interface (GICC_CTLR)
- Set priority mask (GICC_PMR)

**Test**: Initialize GIC, read back control registers

#### Step 2.3: Timer Interrupt Configuration
- Implement `gic_enable_interrupt(27)`  // Virtual timer
- Set priority via GICD_IPRIORITYR
- Route to CPU0 via GICD_ITARGETSR (GICv2)

**Test**: Enable timer IRQ 27, verify GICD_ISENABLER[0] bit 27 is set

### Phase 3: IRQ Exception Handling (Interrupt Reception)

**Goal**: Handle timer interrupts in exception handler

#### Step 3.1: Stub IRQ Handler
- Modify `exceptions.S` IRQ vectors
- Save minimal context (just enough for testing)
- Call simple C function `irq_handler_stub()`
- Print message, acknowledge interrupt at GIC
- Restore context and ERET

**Test**: Enable interrupts (msr daifclr, #2), verify IRQ handler is called

#### Step 3.2: GIC Interrupt Acknowledgement
- Implement `gic_acknowledge_interrupt()`: Read GICC_IAR
- Implement `gic_end_interrupt(uint32_t irq_id)`: Write GICC_EOIR
- Add IRQ dispatch logic (switch on IRQ ID)

**Test**: Verify timer IRQs are acknowledged and EOI'd correctly

#### Step 3.3: Timer Reload in IRQ Handler
- Add `timer_reload()` function
- Call from timer IRQ handler
- Verify periodic interrupts (10ms interval)

**Test**: Count timer interrupts, verify frequency matches quantum

### Phase 4: Interrupt Enable/Disable (Critical Sections)

**Goal**: Implement proper interrupt masking for data structure protection

#### Step 4.1: Implement Interrupt Functions
- Implement `interrupts_enable()`: `msr daifclr, #2`
- Implement `interrupts_disable()`: `msr daifset, #2`
- Add `interrupts_save()` and `interrupts_restore()`

**Test**: Verify DAIF register changes correctly

#### Step 4.2: Protect Scheduler Data Structures
- Add interrupt disable/enable around scheduler queue operations
- Protect `scheduler_add()`, `scheduler_remove()`

**Test**: Add processes with interrupts enabled, verify no corruption

### Phase 5: Scheduler Integration (Context Switching)

**Goal**: Call scheduler from timer interrupt

#### Step 5.1: Full IRQ Context Save
- Modify IRQ handler in `exceptions.S`
- Save full context (36 registers, same as sync handler)
- Pass context pointer to C handler

**Test**: Verify context frame layout in IRQ handler

#### Step 5.2: Call Scheduler from IRQ Handler
- Add `timer_irq_handler(cpu_context_t *ctx)`
- Call `scheduler_schedule_with_context(ctx)`
- Return new stack pointer
- IRQ handler restores new context and ERETsto next process

**Test**: Create single process, verify it resumes after timer IRQ

#### Step 5.3: Multi-Process Context Switching
- Create two test processes (simple counters)
- Add to scheduler
- Enable timer interrupts
- Verify alternating execution

**Test**: Verify both processes make progress, counters increment

### Phase 6: Thread1 and Thread2 Integration (Main Goal)

**Goal**: Enable `thread1()` and `thread2()` from `main.c`

#### Step 6.1: Create Thread Processes
- In `main()`, after `scheduler_init()`:
  - `process_t *p1 = process_create(thread1, ...)`
  - `process_t *p2 = process_create(thread2, ...)`
- Add both processes to scheduler:
  - `scheduler_add(p1)`
  - `scheduler_add(p2)`

**Test**: Verify processes are created and added to run queue

#### Step 6.2: Initialize Interrupts
- Call `timer_init()` - set 10ms quantum
- Call `gic_init()` - initialize GIC
- Call `gic_enable_interrupt(27)` - enable timer IRQ
- Call `interrupts_enable()` - unmask IRQs

**Test**: Verify timer interrupts are firing

#### Step 6.3: Enable Scheduling
- Remove `while(1) wfe` loop from main
- Instead, call `scheduler_schedule_with_context(NULL)` to bootstrap
- Never return to main

**Test**: Verify thread1 and thread2 alternate execution

#### Step 6.4: Verify Output
- Run system
- Verify console shows alternating output:
  ```
  [Thread 1] Count: 0
  [Thread 2] Count: 0
  [Thread 1] Count: 1
  [Thread 2] Count: 1
  ...
  ```

**Test**: Run for 10+ seconds, verify stable operation

### Phase 7: Cleanup and Refinement (Polish)

#### Step 7.1: Process Termination
- Implement `process_reap()` to free memory
- Call from scheduler when process state is TERMINATED
- Test with `sys_exit()` syscall

**Test**: Create process that exits, verify memory is freed

#### Step 7.2: Idle Process
- Create special idle process (PID 0)
- Runs when no other process is READY
- Simply executes `wfe` instruction

**Test**: Let thread1 and thread2 exit, verify idle process runs

#### Step 7.3: Statistics and Debugging
- Add context switch counter
- Add per-process run time tracking
- Add `scheduler_dump()` for debugging

**Test**: Print statistics after running threads

---

## References

### Official ARM Documentation
- [AArch64 Generic Timer Programmer's Guide](https://tc.gts3.org/cs3210/2020/spring/r/aarch64-generic-timer.pdf)
- [Learn the Architecture - AArch64 Exception Model](https://documentation-service.arm.com/static/63a065c41d698c4dc521cb1c)
- [ARM Exception Model](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Exception%20model.pdf)
- [CNTV_TVAL_EL0 Register](https://developer.arm.com/documentation/101550/latest/AArch64-registers/AArch64-register-descriptions/AArch64-Generic-Timer-register-description/CNTV-TVAL-EL0--Counter-timer-Virtual-Timer-TimerValue-Register)
- [DAIF: Interrupt Mask Bits](https://developer.arm.com/documentation/ddi0595/2021-06/AArch64-Registers/DAIF--Interrupt-Mask-Bits)
- [ARM GIC Architecture Specification](https://www.scs.stanford.edu/~zyedidia/docs/arm/gic_v3.pdf)
- [Learn the Architecture - GIC](https://developer.arm.com/docs/den0024/latest/aarch64-exception-handling/the-generic-interrupt-controller)

### Educational Resources
- [Lab 4: Preemptive Multitasking (CS-3210)](https://tc.gts3.org/cs3210/2020/spring/lab/lab4.html)
- [raspberry-pi-os: Scheduler Tutorial](https://s-matyukevich.github.io/raspberry-pi-os/docs/lesson04/rpi-os.html)
- [ARMv-8a EL0<->EL1 Switching](https://pyjamacafe.com/posts/arm64-day1-el0-el1-switching/)
- [AArch64 Exception Levels](https://medium.com/@om.nara/aarch64-exception-levels-60d3a74280e6)
- [AArch64 Interrupt Handling (krinkinmu)](https://krinkinmu.github.io/2021/01/10/aarch64-interrupt-handling.html)

### Open Source Implementations
- [s-matyukevich/raspberry-pi-os](https://github.com/s-matyukevich/raspberry-pi-os) - Comprehensive RPi OS tutorial
- [littlekernel/lk - ARM Generic Timer](https://github.com/littlekernel/lk/blob/master/dev/timer/arm_generic/arm_generic_timer.c)
- [cloudius-systems/osv - ARM Clock](https://github.com/cloudius-systems/osv/blob/master/arch/aarch64/arm-clock.cc)
- [umanovskis/baremetal-arm - Interrupts](https://github.com/umanovskis/baremetal-arm/blob/master/doc/07_interrupts.md)
- [rhythm16/rpi4-bare-metal](https://github.com/rhythm16/rpi4-bare-metal) - RPi4 OS with scheduler

### Forums and Community Resources
- [Interrupts on RPI3 in aarch64 mode](https://forums.raspberrypi.com/viewtopic.php?t=188366)
- [Implementing Bare-Metal GICv3 on AArch64](https://www.systemonchips.com/implementing-bare-metal-gicv3-on-aarch64-irq-handling-and-timer-configuration-challenges/)
- [AArch64 GIC and Timer Interrupt (Löwenware)](https://lowenware.com/blog/aarch64-gic-and-timer-interrupt/)

### Linux Kernel Documentation
- [ARM Architecture Timer (Device Tree)](https://www.kernel.org/doc/Documentation/devicetree/bindings/timer/arm,arch_timer.txt)
- [ARM Architecture Timer (YAML)](https://www.kernel.org/doc/Documentation/devicetree/bindings/timer/arm,arch_timer.yaml)

---

## Appendix: Key Concepts Summary

### Preemptive vs Cooperative Multitasking

**Cooperative**: Process voluntarily yields CPU (via syscall)
- Simpler to implement
- No need for timer interrupts
- Risk: Buggy process can monopolize CPU

**Preemptive**: OS forcibly switches processes via timer interrupt
- More complex (requires timer + IRQ handling)
- Fair CPU time distribution
- Better system responsiveness

### Round-Robin Scheduling

- **Algorithm**: Circular queue of processes
- **Time Quantum**: Fixed time slice (e.g., 10ms)
- **Fairness**: Each process gets equal CPU time
- **Implementation**: Linked list, advance to next on timer IRQ

### Exception-Driven Context Switch

1. Timer interrupt occurs (IRQ exception)
2. CPU automatically saves PC → ELR_EL1, PSTATE → SPSR_EL1
3. IRQ handler saves full context (31 GPRs, SP, TTBR0) to current process stack
4. Scheduler selects next process
5. Scheduler loads next process context onto its stack
6. IRQ handler restores new context (x0-x30, SP, TTBR0)
7. ERET restores PC and PSTATE, jumps to new process

### Critical Sections

Regions of code that must not be interrupted:

```c
// Protect scheduler queue modification
interrupts_disable();
scheduler_add(process);
interrupts_enable();
```

Without protection, timer interrupt during queue modification could corrupt data structures.

---

*Document Version: 1.0*
*Last Updated: 2026-01-12*
*Author: Slopix Development Team*
