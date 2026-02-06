# Consolidate Duplicate Assembly Helper Functions

## Severity

High

## Files Affected

- `/Users/davidklassen/work/davidklassen/slopix/kernel/build.c` (lines 68-81)
- `/Users/davidklassen/work/davidklassen/slopix/boot/build.c` (lines 21-34)

## Description

The functions `kernel_assemble()` and `boot_assemble()` are identical implementations. Both functions perform the exact same operation: invoke the assembler on a `.S` source file and produce an object file in `.build/obj/`.

### Current Implementation

**kernel/build.c (lines 68-81):**
```c
static int kernel_assemble(const char *src) {
	const char *as = get_env_or("AS", "as");
	char srcfile[256], ofile[256];

	snprintf(srcfile, sizeof(srcfile), "%s.S", src);
	snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", src);

	Cmd cmd = {0};
	cmd_append(&cmd, as, srcfile, "-o", ofile, NULL);
	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}
```

**boot/build.c (lines 21-34):**
```c
static int boot_assemble(const char *src) {
	const char *as = get_env_or("AS", "as");
	char srcfile[256], ofile[256];

	snprintf(srcfile, sizeof(srcfile), "%s.S", src);
	snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", src);

	Cmd cmd = {0};
	cmd_append(&cmd, as, srcfile, "-o", ofile, NULL);
	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}
```

Both implementations are byte-for-byte identical with no contextual differences.

## Risk of Divergence

- **Maintenance burden**: Any future bug fixes or improvements to assembly compilation must be applied in two places
- **Inconsistency risk**: Changes made to one function without corresponding updates to the other could cause divergent behavior between kernel and bootloader builds
- **Testing gaps**: Fixes verified in one build system may not be validated in the other, potentially introducing subtle bugs

## Recommended Solution

Extract the duplicate function into a shared helper in `lib/build.h`:

1. Add `int assemble_src(const char *src)` to the public interface in `lib/build.h` header section
2. Implement the function in the `#ifdef BUILD_IMPLEMENTATION` section of `lib/build.h` (around line 485 where the existing `assemble()` function is defined)
3. Replace both `kernel_assemble()` and `boot_assemble()` with calls to the shared function
4. Remove the duplicate static functions from both `kernel/build.c` and `boot/build.c`

**Note**: The proposed helper function name differs from the existing `assemble()` function to avoid confusion. The existing `assemble()` in lib/build.h handles path basename extraction and `.S` extension detection. The new helper should either be named consistently or the existing `assemble()` should be used if it serves the same purpose.

## Test Recommendations

1. Verify `make clean && make test` passes (kernel tests)
2. Verify `make clean` && `cd boot && make` produces bootloader.bin with identical size/checksum before and after consolidation
3. Run both builds with `AS`, `CC`, and `LD` environment variables overridden to ensure the shared helper respects all environment configurations
4. Spot-check generated object files are identical to pre-refactoring builds

## Implementation Notes

- Both functions appear in private (`static`) scope, so refactoring will not affect the public API
- The shared helper should remain simple and focused on the core assembly operation
- Consider whether the existing `assemble()` function in lib/build.h can be reused or if a new purpose-specific helper is more appropriate
