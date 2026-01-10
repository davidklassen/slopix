# QEMU virt Machine (aarch64) Platform Specification

**Source**: QEMU source code `hw/arm/virt.c` (base_memmap array)
**Machine Type**: `qemu-system-aarch64 -M virt`

## Memory Map

The QEMU virt machine defines the following memory layout:

| Device               | Base Address | Size       | Notes                           |
|---------------------|--------------|------------|---------------------------------|
| Flash               | 0x00000000   | 128 MB     | Boot firmware region            |
| GIC Distributor     | 0x08000000   | 64 KB      | GICv2 GICD registers            |
| GIC CPU Interface   | 0x08010000   | 64 KB      | GICv2 GICC registers            |
| GIC V2M             | 0x08020000   | 4 KB       | MSI frame                       |
| GIC Hyp             | 0x08030000   | 64 KB      | Hypervisor interface            |
| GIC vCPU            | 0x08040000   | 64 KB      | Virtual CPU interface           |
| GIC ITS             | 0x08080000   | 128 KB     | GICv3 interrupt translation     |
| GIC Redistributor   | 0x080A0000   | 15.375 MB  | GICv3 redistributor             |
| UART0 (PL011)       | 0x09000000   | 4 KB       | Primary serial console          |
| RTC (PL031)         | 0x09010000   | 4 KB       | Real-time clock                 |
| Firmware Config     | 0x09020000   | 24 bytes   | fw_cfg device                   |
| GPIO (PL061)        | 0x09030000   | 4 KB       | GPIO controller                 |
| UART1 (PL011)       | 0x09040000   | 4 KB       | Secondary serial                |
| SMMU                | 0x09050000   | 128 KB     | System MMU                      |
| **RAM**             | **0x40000000** | Varies   | **Main memory (default 128MB)** |

## Important Notes

1. **RAM Base**: Physical RAM starts at 0x40000000 (1GB offset)
2. **RAM Size**: Configurable via `-m` flag (e.g., `-m 128M`)
3. **Maximum RAM**: Up to 255GB supported
4. **Device Tree**: QEMU generates a device tree blob (DTB) describing the platform
5. **Variability**: Device locations above may change between QEMU versions; always consult DTB for production code

## GICv2 Configuration

For interrupt controller (used in SLOPIX):
- **GICD Base**: 0x08000000 (distributor registers)
- **GICC Base**: 0x08010000 (CPU interface registers)

## UART Configuration

For serial console (used in SLOPIX):
- **UART0 Base**: 0x09000000 (PL011 UART)
- **Baud Rate**: Typically 115200 (configurable)

## References

- QEMU virt machine source: `hw/arm/virt.c`
- Official documentation: https://qemu-project.gitlab.io/qemu/system/arm/virt.html
- Device tree can be dumped with: `qemu-system-aarch64 -M virt,dumpdtb=virt.dtb`
