# ARM GICv2 (Generic Interrupt Controller v2) Register Specification

**Source**: ARM Generic Interrupt Controller Architecture Specification v2.0
**Used in**: QEMU virt machine for aarch64

## Overview

The GIC has two main register blocks:
- **GICD** (Distributor): Controls interrupt distribution and routing
- **GICC** (CPU Interface): Per-CPU interrupt handling interface

In QEMU virt machine:
- GICD base address: 0x08000000
- GICC base address: 0x08010000

---

## GICD - Distributor Registers

Base Address: 0x08000000 (in QEMU virt)

### Control and Status

| Offset | Register       | Name                              | Description                        |
|--------|----------------|-----------------------------------|------------------------------------|
| 0x000  | GICD_CTLR      | Distributor Control Register      | Enable/disable distributor         |
| 0x004  | GICD_TYPER     | Interrupt Controller Type Register| Number of interrupt lines, CPUs    |
| 0x008  | GICD_IIDR      | Distributor Implementer ID        | Implementation identification      |

#### GICD_CTLR (0x000)

| Bit(s) | Field      | Description                              |
|--------|------------|------------------------------------------|
| 0      | Enable     | Enable distributor (0=disabled, 1=enabled)|
| 31:1   | Reserved   | RES0                                     |

---

### Interrupt Security (Optional)

| Offset       | Register        | Name                          | Description                    |
|--------------|-----------------|-------------------------------|--------------------------------|
| 0x080 + 4*n  | GICD_IGROUPRn   | Interrupt Group Registers     | Security state (Group 0/1)     |

Each bit controls one interrupt (32 interrupts per register):
- Bit = 0: Group 0 (Secure)
- Bit = 1: Group 1 (Non-secure)

---

### Interrupt Enable/Disable

| Offset       | Register         | Name                            | Description                  |
|--------------|------------------|---------------------------------|------------------------------|
| 0x100 + 4*n  | GICD_ISENABLERn  | Interrupt Set-Enable Registers  | Enable interrupts            |
| 0x180 + 4*n  | GICD_ICENABLERn  | Interrupt Clear-Enable Registers| Disable interrupts           |

Each bit controls one interrupt (32 interrupts per register):
- Write 1 to bit N: Enable/disable interrupt N
- Write 0: No effect
- Read: Current enable state

**Example**: To enable interrupt 30:
```
GICD_ISENABLER(0) = (1 << 30)  // 0x100 + 0*4 = 0x100
```

---

### Interrupt Pending

| Offset       | Register        | Name                            | Description                   |
|--------------|-----------------|---------------------------------|-------------------------------|
| 0x200 + 4*n  | GICD_ISPENDRn   | Interrupt Set-Pending Registers | Set interrupt pending         |
| 0x280 + 4*n  | GICD_ICPENDRn   | Interrupt Clear-Pending Registers| Clear interrupt pending      |

Each bit controls one interrupt (32 interrupts per register):
- Write 1 to bit N: Set/clear pending state for interrupt N
- Read: Current pending state

---

### Interrupt Active

| Offset       | Register         | Name                             | Description                  |
|--------------|------------------|----------------------------------|------------------------------|
| 0x300 + 4*n  | GICD_ISACTIVERn  | Interrupt Set-Active Registers   | Set interrupt active         |
| 0x380 + 4*n  | GICD_ICACTIVERn  | Interrupt Clear-Active Registers | Clear interrupt active       |

---

### Interrupt Priority

| Offset       | Register          | Name                           | Description                    |
|--------------|-------------------|--------------------------------|--------------------------------|
| 0x400 + 4*n  | GICD_IPRIORITYRn  | Interrupt Priority Registers   | Set interrupt priority (0-255) |

Each register contains priorities for 4 interrupts (1 byte per interrupt):
- Lower values = higher priority
- 8-bit priority field (but may implement fewer bits)
- Byte N controls interrupt (register_num * 4) + N

**Example**: To set priority 0xA0 for interrupt 30:
```
GICD_IPRIORITYR(7) = 0xA0 << 16  // 0x400 + 7*4, interrupt 30 = byte 2
```

---

### Interrupt Targets

| Offset       | Register        | Name                           | Description                     |
|--------------|-----------------|--------------------------------|---------------------------------|
| 0x800 + 4*n  | GICD_ITARGETSRn | Interrupt Processor Targets    | CPU target list for interrupt   |

Each register contains target CPU masks for 4 interrupts (1 byte per interrupt):
- Each bit represents one CPU (bit 0 = CPU 0, etc.)
- Byte N controls interrupt (register_num * 4) + N
- Interrupt is sent to all CPUs with their bit set

**Example**: Route interrupt 30 to CPU 0:
```
GICD_ITARGETSR(7) = 0x01 << 16  // 0x800 + 7*4, interrupt 30 = byte 2, CPU 0 = bit 0
```

---

### Interrupt Configuration

| Offset       | Register     | Name                            | Description                       |
|--------------|--------------|---------------------------------|-----------------------------------|
| 0xC00 + 4*n  | GICD_ICFGRn  | Interrupt Configuration Registers| Edge/level triggered configuration|

Each register configures 16 interrupts (2 bits per interrupt):
- Bits [1:0] for interrupt (register_num * 16) + 0
- Bits [3:2] for interrupt (register_num * 16) + 1
- etc.

Configuration bits:
- Bit 0: Reserved (RES0)
- Bit 1: 0 = level-sensitive, 1 = edge-triggered

---

### Software Generated Interrupts

| Offset | Register    | Name                                | Description                  |
|--------|-------------|-------------------------------------|------------------------------|
| 0xF00  | GICD_SGIR   | Software Generated Interrupt Register| Generate SGI to target CPUs  |

| Bits   | Field           | Description                              |
|--------|-----------------|------------------------------------------|
| 3:0    | SGIINTID        | SGI interrupt ID (0-15)                  |
| 23:16  | CPUTargetList   | Target CPU list (1 bit per CPU)          |
| 25:24  | TargetListFilter| 0=use list, 1=all but self, 2=self only |

---

## GICC - CPU Interface Registers

Base Address: 0x08010000 (in QEMU virt, per-CPU)

### Control and Status

| Offset | Register    | Name                          | Description                        |
|--------|-------------|-------------------------------|------------------------------------|
| 0x0000 | GICC_CTLR   | CPU Interface Control Register| Enable/disable CPU interface       |
| 0x0004 | GICC_PMR    | Interrupt Priority Mask       | Priority mask for interrupt delivery|
| 0x0008 | GICC_BPR    | Binary Point Register         | Priority grouping                  |
| 0x000C | GICC_IAR    | Interrupt Acknowledge Register| Read to acknowledge interrupt      |
| 0x0010 | GICC_EOIR   | End of Interrupt Register     | Write to signal interrupt complete |
| 0x0014 | GICC_RPR    | Running Priority Register     | Highest priority active interrupt  |
| 0x0018 | GICC_HPPIR  | Highest Priority Pending IRQ  | Highest priority pending interrupt |

---

### GICC_CTLR (0x0000)

| Bit(s) | Field      | Description                              |
|--------|------------|------------------------------------------|
| 0      | Enable     | Enable CPU interface (0=disabled, 1=enabled)|
| 31:1   | Reserved   | RES0                                     |

---

### GICC_PMR (0x0004)

Priority Mask Register - only interrupts with higher priority than this value are delivered.

| Bit(s) | Field      | Description                              |
|--------|------------|------------------------------------------|
| 7:0    | Priority   | Priority mask value (0-255)              |
| 31:8   | Reserved   | RES0                                     |

- Lower values = higher priority
- Set to 0xFF to allow all interrupts
- Set to 0x00 to mask all interrupts

---

### GICC_IAR (0x000C)

Interrupt Acknowledge Register - read to get the interrupt ID and acknowledge it.

| Bit(s) | Field      | Description                              |
|--------|------------|------------------------------------------|
| 9:0    | InterruptID| Interrupt ID (0-1019)                    |
| 12:10  | CPUID      | Source CPU ID for SGIs                   |
| 31:13  | Reserved   | RES0                                     |

**Special Values**:
- 1023: Spurious interrupt (no pending interrupt)
- 1022: Reserved

**Usage**: Read this register to get pending interrupt ID. This automatically acknowledges the interrupt and changes its state from pending to active.

---

### GICC_EOIR (0x0010)

End of Interrupt Register - write to signal completion of interrupt handling.

| Bit(s) | Field      | Description                              |
|--------|------------|------------------------------------------|
| 9:0    | EOIINTID   | Interrupt ID to complete                 |
| 12:10  | CPUID      | Source CPU ID (for SGIs)                 |
| 31:13  | Reserved   | RES0                                     |

**Usage**: Write the interrupt ID from GICC_IAR to this register after handling the interrupt. This deactivates the interrupt.

---

## Typical Interrupt Handling Flow

### Initialization

1. Disable distributor: `GICD_CTLR = 0`
2. Configure interrupts:
   - Disable all: `GICD_ICENABLER(n) = 0xFFFFFFFF`
   - Set priorities: `GICD_IPRIORITYR(n) = ...`
   - Set targets: `GICD_ITARGETSR(n) = 0x01010101` (CPU 0)
   - Configure type: `GICD_ICFGR(n) = ...`
3. Enable distributor: `GICD_CTLR = 1`
4. Enable CPU interface: `GICC_CTLR = 1`
5. Set priority mask: `GICC_PMR = 0xFF`
6. Enable specific interrupts: `GICD_ISENABLER(n) = (1 << irq)`

### Handling an Interrupt

1. Read `GICC_IAR` to get interrupt ID (auto-acknowledges)
2. Handle the interrupt
3. Write interrupt ID to `GICC_EOIR` to complete

**Example**:
```c
unsigned int irq = GICC_IAR;
if (irq == 1023) {
    // Spurious interrupt
    return;
}

// Handle interrupt irq
handle_interrupt(irq);

// Signal completion
GICC_EOIR = irq;
```

---

## References

- ARM Generic Interrupt Controller Architecture Specification v2.0
- ARM GIC-400 Technical Reference Manual
- QEMU source: `hw/intc/arm_gic.c`
