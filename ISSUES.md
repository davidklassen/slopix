# Known Issues

## Timer Interrupts Not Preempting User Process

**Status:** Ready to investigate (TTBR0 issue fixed)

**Symptom:**
After entering user mode (EL0), timer interrupts don't fire. The user process runs its infinite loop but is never preempted. Kernel processes (cursor_blink, prompt) never get scheduled.

**Debug findings:**

1. Timer and GIC work correctly in kernel mode (tests pass)
2. User process enters EL0 successfully via `usertrap_return`
3. SPSR = 0 before ERET, meaning DAIF = 0 (all interrupts unmasked) in EL0
4. DAIF in kernel before entering user mode: 0x340 (bit 7 = 0, IRQ enabled)
5. No timer tick debug output appears after entering user mode

**What was verified working:**
- `usertrap_first()` is called and prints debug output
- Trap frame is set up correctly: elr=0x0, sp_el0=0x80000000, spsr=0x0
- Context switch to user process works
- User page table is created and TTBR0 is switched

**What needs investigation:**
- Is the timer still running after TTBR0 switch?
- Is the GIC still delivering interrupts?
- Are the lower EL exception vectors being reached?
- Is there something masking interrupts at the CPU or GIC level?

**Relevant code:**
- `vectors.S:172-177` - `irq_lower_entry` handler
- `timer.c:36-42` - `timer_handler()` with debug output
- `proc.c:61-64` - `usertrap_first()` entry point
- `exception.c:126-141` - `irq_handler()`

---

## TTBR0/Identity Mapping Issue (RESOLVED)

**Status:** Fixed

**Original Symptom:**
After `tlbi vmalle1`, the kernel hung. UART output stopped immediately after the TLB invalidation instruction.

**Root cause:**
The kernel code and data were placed at physical addresses (0x4000xxxx range) by the linker. These addresses are in the TTBR0 range (low addresses), not the TTBR1 kernel range (0xFFFF...). When TTBR0 was switched to a user page table and TLB was invalidated, the kernel code became inaccessible.

**Solution implemented:**
Relocated kernel to run at high virtual addresses (0xFFFF000040000000) via TTBR1.

Key changes:
1. **linker.ld** - Kernel linked at high VA (0xFFFF000040010000) with boot code at physical (0x40000000)
2. **boot.S** - Early boot converts VA symbols to PA, enables MMU, then jumps to high address
3. **early_mmu.S** - New assembly file that sets up page tables using explicit physical address computation (avoids PC-relative adrp issues)
4. **mmu_enable.S** - Moved to .text.boot section to run at physical address
5. **mmu.c** - Page table arrays made non-static for early_mmu.S access
6. **kernel.c** - Removed mmu_init() call (early_mmu_setup handles it)
7. **pmem.c** - Fixed __stack_top to use VA_TO_PA() conversion
8. **proc.c** - Re-enabled tlbi_vmalle1() in scheduler

The kernel now runs entirely from TTBR1 high addresses after MMU enable, allowing TTBR0 to be freely switched between identity mapping and user page tables.
