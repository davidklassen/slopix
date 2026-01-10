# SLOPIX Platform Documentation

This directory contains authoritative platform and architecture documentation used as reference during SLOPIX development. These documents are based on official ARM and QEMU specifications, not our implementation.

## Purpose

- Provide factual, spec-based references to prevent hallucination during development
- Document memory maps, register layouts, and bit fields from official sources
- Serve as quick reference when implementing low-level features

## Documents

### [qemu-virt-platform.md](qemu-virt-platform.md)
Complete memory map for the QEMU virt machine (aarch64), including:
- RAM base address and layout
- Device-mapped I/O regions (UART, GIC, RTC, GPIO, etc.)
- Memory region sizes and boundaries

**Source**: QEMU source code `hw/arm/virt.c`

### [arm64-registers.md](arm64-registers.md)
ARM64 (AArch64) system register specifications for EL1, including:
- SCTLR_EL1 - System control (MMU, caches, alignment)
- TCR_EL1 - Translation control (page table configuration)
- TTBR0_EL1, TTBR1_EL1 - Translation table base addresses
- MAIR_EL1 - Memory attribute indirection
- SPSR_EL1 - Saved processor state
- ELR_EL1 - Exception link register
- ESR_EL1 - Exception syndrome

**Source**: ARM Architecture Reference Manual for ARMv8-A

### [gicv2-registers.md](gicv2-registers.md)
ARM Generic Interrupt Controller v2 register map, including:
- GICD registers (distributor) - interrupt control and routing
- GICC registers (CPU interface) - per-CPU interrupt handling
- Register offsets, bit fields, and usage examples
- Typical interrupt handling flow

**Source**: ARM GIC Architecture Specification v2.0

## Usage

These documents are **references only**. They describe the platform and architecture, not the SLOPIX implementation.

When implementing features:
1. Consult these docs for correct addresses, offsets, and bit layouts
2. Verify against official ARM/QEMU documentation when in doubt
3. Update these docs if you find corrections or omissions

## Not Included

These docs intentionally do NOT cover:
- SLOPIX-specific implementation details (see source code)
- Higher-level concepts or tutorials (see BRIEF.md)
- QEMU usage instructions (see README.md)

Keep these documents factual and implementation-agnostic.
