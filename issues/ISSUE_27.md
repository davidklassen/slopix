# Untested Exception Handling - Tests Execute but Never Assert

## Severity
**Critical**

## File and Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/tests/test_exception.c`
- **Lines:** 5-13 (test functions), 15-18 (test suite)

## Description
The exception handling tests in `test_exception.c` contain dead code that executes exception-triggering instructions (undefined instruction and SVC call) but never verify that exceptions were actually handled. Both test functions unconditionally return 0, meaning they "pass" regardless of whether exception handling works correctly.

### Current Code
```c
TEST(undefined_instruction) {
	asm volatile(".word 0x00000000");
	return 0;
}

TEST(svc_call) {
	asm volatile("svc #42");
	return 0;
}

TEST_SUITE(exception) {
	RUN_TEST(undefined_instruction);
	RUN_TEST(svc_call);
}
```

### Problem
1. **No assertions**: Neither test uses ASSERT, ASSERT_EQ, or any verification macro
2. **Unconditional pass**: Both return 0 immediately after triggering the exception, regardless of what happened
3. **False positive**: If exception handling is broken, crashes the kernel, but the test suite reports success anyway
4. **Silent failures**: No output or state checking to verify handlers executed correctly

The tests trigger valid exceptions (as shown in `/Users/davidklassen/work/davidklassen/slopix/kernel/exception.c`):
- `EC_UNKNOWN` (0x00): Undefined instruction - handler prints "UNDEFINED INSTRUCTION" and skips past instruction
- `EC_SVC_AARCH64` (0x15): SVC call - handler prints "SVC #42" and continues

But the tests never verify these handlers actually ran or set correct state.

## Impact
**This is a critical testing gap.** Exception handling is fundamental kernel infrastructure:

1. **Undetected regressions**: Changes to exception handling won't be caught by tests
2. **Silent system breakage**: Kernel crashes will be misattributed to other components
3. **Dead test code**: Resources spent maintaining tests that provide zero coverage
4. **Difficult debugging**: Real failures come as total system panics, not test failures

### Real-World Scenario
If exception.c handler logic is broken (e.g., ELR not advanced correctly for undefined instruction), the tests still "pass" but kernel immediately crashes on any undefined instruction encountered.

## What Should Be Tested
The exception handlers set CPU and program state that can be verified:

### For `undefined_instruction` test:
1. **Handler detection**: Exception Class (ESR_EL1[31:26]) = 0x00 (EC_UNKNOWN)
2. **State after**: ELR_EL1 should advance by 4 bytes (instruction width)
3. **Program continues**: Execution resumes after the trigger instruction

### For `svc_call` test:
1. **Handler detection**: Exception Class (ESR_EL1[31:26]) = 0x15 (EC_SVC_AARCH64)
2. **Immediate value extraction**: ISS[15:0] should contain 42 (SVC argument)
3. **Handler execution**: Handler processes syscall/SVC flow

## Test Recommendations

### Approach 1: Use Global State (Simplest)
Add a global exception counter before and after, verify exception occurred:

```c
static volatile unsigned int exception_count = 0;

TEST(undefined_instruction) {
	unsigned int count_before = exception_count;
	asm volatile(".word 0x00000000");
	// If we reach here, exception was handled and PC advanced
	unsigned int count_after = exception_count;
	ASSERT_NE(count_before, count_after, "exception not handled");
	return 0;
}
```

Modify sync_exception_handler() to increment `exception_count` when EC_UNKNOWN is detected.

### Approach 2: Verify Register State (More Robust)
Save ELR before and after the undefined instruction to confirm PC advance:

```c
TEST(undefined_instruction) {
	unsigned long elr_before;
	unsigned long elr_after;

	asm volatile("mrs %0, elr_el1" : "=r"(elr_before));
	asm volatile(".word 0x00000000");
	asm volatile("mrs %0, elr_el1" : "=r"(elr_after));

	// Handler should advance PC by 4 (instruction size)
	ASSERT_EQ(elr_after, elr_before + 4, "PC not advanced");
	return 0;
}
```

### Approach 3: Read ESR_EL1 Directly (Lowest Overhead)
Check that ESR_EL1 contains the correct exception code for the triggered exception:

```c
TEST(undefined_instruction) {
	asm volatile(".word 0x00000000");

	unsigned long esr = read_esr_el1();
	unsigned int ec = ESR_EC(esr);
	ASSERT_EQ(ec, EC_UNKNOWN, "ESR_EC not EC_UNKNOWN after undefined instruction");
	return 0;
}

TEST(svc_call) {
	asm volatile("svc #42");

	unsigned long esr = read_esr_el1();
	unsigned int ec = ESR_EC(esr);
	unsigned int iss = ESR_ISS(esr);
	ASSERT_EQ(ec, EC_SVC_AARCH64, "ESR_EC not EC_SVC_AARCH64 after SVC");
	ASSERT_EQ((iss & 0xFFFF), 42, "SVC immediate not 42");
	return 0;
}
```

**Recommended: Approach 2** - Verifies actual behavior (PC advance) that matters most for undefined instruction handling.

## Fixing Recommendation

1. Add test state capture to `sync_exception_handler()` in `/Users/davidklassen/work/davidklassen/slopix/kernel/exception.c` to track exceptions

2. Implement assertions in both test functions using approach from recommendations above

3. Verify both tests execute in test mode:
   - Run `make test` and confirm exception tests actually verify state
   - Both tests should report pass/fail based on assertions, not always pass

4. Document in test that these verify core exception infrastructure works

### Example Fix
```c
TEST(undefined_instruction) {
	unsigned long elr_before;
	unsigned long elr_after;

	// Capture ELR before undefined instruction
	asm volatile("mrs %0, elr_el1" : "=r"(elr_before));

	// Execute undefined instruction - handler will advance PC
	asm volatile(".word 0x00000000");

	// Capture ELR after handler processed exception
	asm volatile("mrs %0, elr_el1" : "=r"(elr_after));

	// Verify handler advanced PC by one instruction (4 bytes in AArch64)
	ASSERT_EQ(elr_after, elr_before + 4, "undefined instruction handler did not advance PC");
	return 0;
}

TEST(svc_call) {
	unsigned long esr;
	unsigned int ec, iss;

	// Execute SVC with immediate 42
	asm volatile("svc #42");

	// Read ESR to verify exception was recorded
	esr = read_esr_el1();
	ec = ESR_EC(esr);
	iss = ESR_ISS(esr);

	// Verify correct exception class was recorded
	ASSERT_EQ(ec, EC_SVC_AARCH64, "SVC exception class not recorded");
	// Verify SVC immediate was captured correctly
	ASSERT_EQ((iss & 0xFFFF), 42, "SVC immediate not captured");
	return 0;
}
```

