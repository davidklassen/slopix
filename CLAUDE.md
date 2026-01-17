# Slopix

Bare-metal AArch64 kernel for QEMU virt board.

## Running QEMU Commands

Always use a 3000ms timeout when running `make run` or `make test`. QEMU may hang if something breaks.

```
make test - exits cleanly via PSCI if tests pass; timeout means failure
make run  - always times out (no exit), kill background shell after
```

## Comments

- Write simple, obvious code that doesn't need comments
- Use // style (except linker scripts which only support /* */)
- Use named constants instead of magic numbers
- Comments are for things that can't be expressed in code: external references, specs, non-obvious "why"

## After Modifying Code

1. `make tidy` - format code
2. `make test` - ensure all tests pass
3. `make run` - verify kernel behaves as expected

## VCS Guidelines

Commit message format:
- First line: one-liner, all lowercase description
- Second line: empty
- Remaining lines: commit body with details

## Test Framework

The test macros in `tests/test.h` are designed to work without `#ifdef` guards in application code:

- When `RUN_TESTS` is defined: macros expand to actual test functions, `TEST_EXIT()` calls `psci_system_off()`
- When `RUN_TESTS` is not defined: all macros become `((void)0)` no-ops

This means `kernel_main()` can use test macros directly without conditionals - in normal builds they simply do nothing and code continues to the next statement.

## Planning

When planning implementation tasks:

1. Search local documentation first via `docsearch` MCP
2. Cross-validate with web search for:
   - Official specs (ARM, OASIS, etc.)
   - Reference implementations (xv6, raspberry-pi-os, seL4)
   - OSDev wiki for practical guidance
3. Cite sources in plan files when referencing specific register layouts or algorithms
