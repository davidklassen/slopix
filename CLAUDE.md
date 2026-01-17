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

## Planning

When planning implementation tasks:

1. Search local documentation first via `docsearch` MCP
2. Cross-validate with web search for:
   - Official specs (ARM, OASIS, etc.)
   - Reference implementations (xv6, raspberry-pi-os, seL4)
   - OSDev wiki for practical guidance
3. Cite sources in plan files when referencing specific register layouts or algorithms
