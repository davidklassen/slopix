# M6 Userspace - Executive Summary

**Date**: 2026-01-11
**Status**: Ready for implementation
**Estimated Effort**: 6.5 hours

---

## Critical Findings

### 🔴 CRITICAL SECURITY BUG
**File**: `memory.h:57`
**Issue**: `PTE_USER_CODE` missing PXN bit
**Impact**: Kernel can execute user code (ret2usr attack vector)
**Fix**: Add `| PTE_PXN` to definition

### 🟠 HIGH PRIORITY BUGS (Blockers for M6)

1. **Exception handlers don't save SP_EL0** (`exceptions.S`)
   - Will corrupt user stack on context switch
   - Need to save/restore SP_EL0 and TTBR0_EL1

2. **Context frame size inconsistency**
   - Header says 36, code uses 34
   - Will corrupt stack when SP_EL0/TTBR0 added

3. **No syscall dispatcher** (`interrupts.c`)
   - SVC instruction causes halt instead of syscall
   - Need to check EC=0x15 and dispatch

4. **No separate EL0 handlers** (`exceptions.S`)
   - All exception vectors jump to same code
   - Need distinct handlers at offset 0x400+

5. **Scheduler doesn't switch page tables** (`scheduler.c`)
   - All processes share same TTBR0_EL1
   - Need to update TTBR0_EL1 on context switch

---

## Implementation Plan (10 Steps)

### Phase 1: Foundation (2 steps, 35 min)
| Step | Task | Time | Risk |
|------|------|------|------|
| 1.1 | Fix PTE_USER_CODE security bug | 5 min | LOW |
| 1.2 | Add SP_EL0/TTBR0 save/restore (36-register frame) | 30 min | MED |

### Phase 2: Syscalls (2 steps, 1 hour)
| Step | Task | Time | Risk |
|------|------|------|------|
| 2.1 | Detect SVC instruction (EC=0x15) | 15 min | LOW |
| 2.2 | Implement syscall dispatcher (exit/write/getpid) | 45 min | MED |

### Phase 3: EL0 Execution (3 steps, 2.25 hours)
| Step | Task | Time | Risk |
|------|------|------|------|
| 3.1 | Create process_create_user() with dual stacks | 45 min | MED |
| 3.2 | Add separate EL0 exception handlers (offset 0x400) | 1 hr | HIGH |
| 3.3 | Test first EL0 process ("Hello from EL0!") | 30 min | MED |

### Phase 4: Memory Isolation (1 step, 1.5 hours)
| Step | Task | Time | Risk |
|------|------|------|------|
| 4.1 | Per-process page tables (TTBR0_EL1) | 1.5 hr | HIGH |

### Phase 5: Polish (2 steps, 1.25 hours)
| Step | Task | Time | Risk |
|------|------|------|------|
| 5.1 | User pointer validation | 30 min | LOW |
| 5.2 | Integration test (multiple processes) | 45 min | LOW |

**Total**: 10 steps, ~6.5 hours

---

## Quick Start

### 1. Fix Security Bug (5 minutes)
```c
// File: memory.h:57
-#define PTE_USER_CODE    (PTE_USER)
+#define PTE_USER_CODE    (PTE_USER | PTE_PXN)

// File: tests/test_pte_bits.c:53-54
-// PXN = 0 (but wait, should be 1 for security!)
-ASSERT_EQ((pte >> 53) & 1, 0);
+// PXN = 1 (security: kernel cannot execute user code)
+ASSERT_EQ((pte >> 53) & 1, 1);
```

```bash
make test && make run-test
# Verify: "PTE_USER_CODE has correct attributes: PASS"
```

### 2. Extend Context Frame (30 minutes)
Update exception handlers to save 36 registers (was 34):
- Add SP_EL0 and TTBR0_EL1 save/restore
- Update scheduler to use 36-register frame
- Update process.c stack calculation

See `M6_ROADMAP.md` Step 1.2 for detailed code changes.

### 3. Continue with roadmap...

---

## Success Criteria

M6 complete when:
- ✅ EL0 process runs and prints "Hello from EL0!"
- ✅ Syscalls work (exit, write, getpid)
- ✅ Each process has own page table
- ✅ Context switching preserves SP_EL0/TTBR0_EL1
- ✅ No regressions (M1-M5 tests pass)

---

## Key ARM64 Concepts

### Exception Levels
- **EL0**: Userspace (unprivileged)
- **EL1**: Kernel (privileged)
- **Transition**: EL0→EL1 via SVC/IRQ, EL1→EL0 via ERET

### Dual Stack Architecture
- **SP_EL0**: User stack (at EL0)
- **SP_EL1**: Kernel stack (at EL1, during syscalls)
- **Critical**: Hardware does NOT auto-save SP_EL0

### Exception Vectors (VBAR_EL1 + offset)
- **0x200**: Current EL (SPx) - EL1→EL1
- **0x400**: Lower EL (AArch64) - EL0→EL1 ← **SYSCALLS HERE**

### Page Tables (Memory Isolation)
- **TTBR0_EL1**: Per-process (user) page table
- **TTBR1_EL1**: Kernel page table (shared)
- **Critical**: Must update TTBR0_EL1 on context switch

### Syscall Mechanism
1. User: `mov x8, #64; svc #0` (SYS_write)
2. Hardware: Save PC→ELR_EL1, PSTATE→SPSR_EL1, jump to VBAR+0x400
3. Handler: Save registers, check EC=0x15, dispatch
4. Syscall: Execute (x8=number, x0-x7=args, x0=return)
5. Handler: Restore registers
6. Hardware (ERET): Restore PC, PSTATE, return to EL0

---

## Testing Strategy

```bash
# After each step:
make clean
make test
make run-test

# Verify:
# - No crashes or hangs
# - Expected output matches
# - All tests PASS
```

### Key Tests
- `test_pte_bits.c` - PXN bit verification
- `test_context_frame.c` - 36-register frame
- `test_svc_detection.c` - SVC instruction
- `test_syscall_dispatch.c` - Argument passing
- `test_el0_hello.c` - First EL0 execution
- `test_el0_integration.c` - Multiple processes

---

## Risk Mitigation

### High-Risk Steps
1. **Step 3.2** (EL0 exception handlers)
   - Complex assembly, easy to corrupt stack
   - **Mitigate**: Test with EL1 first, add debug prints

2. **Step 4.1** (Page tables)
   - Incorrect mappings cause data aborts
   - **Mitigate**: Start with identity mapping, test incrementally

3. **Step 1.2** (Context frame resize)
   - Size mismatch corrupts stack
   - **Mitigate**: Use CONTEXT_FRAME_SIZE constant everywhere

### Debug Tips
```c
// Add debug logging:
printf("[DEBUG] sp_el0=0x%lx, ttbr0=0x%lx\n", sp_el0, ttbr0);
printf("[DEBUG] Stack alignment: 0x%lx (should be 0x...0 or 0x...8)\n", sp & 0xF);
```

---

## Documentation

- **Full Roadmap**: `M6_ROADMAP.md` (detailed implementation)
- **ARM Guide**: `docs/arm64-userspace-el0.md` (983 lines)
- **Dual-Stack**: `docs/dual-stack-architecture.md` (940 lines)
- **Page Tables**: `docs/arm64-page-tables.md`

---

## Beyond M6

### M7: Fork & Exec
- Copy-on-write
- ELF loader
- Process spawning from userspace

### M8: Filesystem
- VFS layer
- initramfs
- File I/O syscalls

### M9: Shell
- Terminal driver
- Line editing
- Interactive shell

---

**Ready to implement!** Start with Step 1.1 (security bug fix).
